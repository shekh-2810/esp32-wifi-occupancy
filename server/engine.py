"""Polling engine: pulls data from every node, runs the pipeline, drives the displays."""
import statistics
import threading
import time
from collections import deque

import requests

import config
import fusion
import storage

NODE_REQUEST_TIMEOUT_S = 2
ACTUATOR_TIMEOUT_S = 1


class OccupancyEngine:
    def __init__(self):
        self._lock = threading.Lock()
        self.history = deque(maxlen=config.HISTORY_SIZE)
        self.presence = fusion.PresenceTracker()

        self.display_mode = config.MODE_COMBINED
        self._last_clap_count = None  # None until the first read sets a baseline

        self._latest = {
            "room_count": 0, "raw_count": 0, "candidate_count": 0, "confidence": 0,
            "mean_rssi": 0, "rssi_stddev": 0, "node_agreement": 0, "probe_rate": 0,
            "unusual_jump": False, "jump_score": 0,
            "display_mode": self.display_mode,
            "per_node_status": {}, "fused_devices": [],
            "rssi_threshold": config.RSSI_PRESENCE_THRESHOLD,
            "last_update": None,
        }

    # ---- public API used by Flask ------------------------------------------

    def snapshot(self, history_limit=300) -> dict:
        with self._lock:
            result = dict(self._latest)
            result["history"] = list(self.history)[-history_limit:]
            return result

    def full_history(self) -> list:
        with self._lock:
            return list(self.history)

    def run_forever(self) -> None:
        while True:
            try:
                self.poll_once()
            except Exception as exc:  # keep the loop alive no matter what
                print("[POLL ERROR]", exc)
            time.sleep(config.POLL_INTERVAL_S)

    # ---- one polling cycle ---------------------------------------------------

    def poll_once(self) -> None:
        node_data, per_node_status, total_probes = self._poll_nodes()

        fused = fusion.fuse_devices(node_data)
        node_counts = [s["devices"] for s in per_node_status.values() if s["ok"]]
        rssi_values = [d["rssi"] for d in fused if isinstance(d.get("rssi"), int)]

        mean_rssi = statistics.mean(rssi_values) if rssi_values else 0
        rssi_stddev = statistics.stdev(rssi_values) if len(rssi_values) > 1 else 0

        confirmed, candidates = self.presence.update(fused)
        room_count = confirmed
        timestamp = time.time()

        # None (not 0) for a node that failed, so the chart shows a gap, not a false drop.
        node_counts_by_name = {
            name: (s["devices"] if s["ok"] else None) for name, s in per_node_status.items()
        }
        with self._lock:
            self.history.append({
                "timestamp": timestamp,
                "occupancy": room_count,
                "raw_count": len(fused),
                "node_counts": node_counts,
                "node_counts_by_name": node_counts_by_name,
                "mean_rssi": mean_rssi,
                "probe_rate": total_probes,
            })

        confidence = fusion.calculate_confidence(
            len(node_counts), len(config.NODES), fused, node_counts, rssi_values)
        agreement = fusion.node_agreement(node_counts)
        unusual_jump, jump_score = fusion.detect_unusual_jump(self.history, room_count)

        self._update_mode_from_claps()
        self._push_display(per_node_status, node_counts, room_count)

        # A DB failure must never block the live stats update below.
        try:
            storage.persist_row(
                timestamp, len(fused), confirmed, candidates, node_counts,
                mean_rssi, rssi_stddev, total_probes, confidence, round(agreement * 100, 1))
        except Exception as exc:
            print("[DB WRITE ERROR - continuing without persistence this cycle]", exc)

        with self._lock:
            self._latest.update({
                "room_count": room_count,
                "raw_count": len(fused),
                "candidate_count": candidates,
                "confidence": confidence,
                "display_mode": self.display_mode,
                "mean_rssi": round(mean_rssi, 1),
                "rssi_stddev": round(rssi_stddev, 1),
                "node_agreement": round(agreement * 100, 1),
                "probe_rate": total_probes,
                "unusual_jump": unusual_jump,
                "jump_score": jump_score,
                "per_node_status": per_node_status,
                "fused_devices": fused,
                "rssi_threshold": config.RSSI_PRESENCE_THRESHOLD,
                "last_update": timestamp,
            })

    # ---- steps ----------------------------------------------------------------

    def _poll_nodes(self):
        node_data, per_node_status, total_probes = {}, {}, 0

        for name, base_url in config.NODES.items():
            try:
                resp = requests.get(f"{base_url}/api/devices", timeout=NODE_REQUEST_TIMEOUT_S)
                resp.raise_for_status()
                data = resp.json()
                now_ms = data.get("now_ms", 0)

                valid = []
                for dev in data.get("devices", []):
                    try:
                        if now_ms - int(dev["last_seen_ms"]) <= config.STALE_AFTER_MS:
                            valid.append(dev)
                    except (KeyError, TypeError, ValueError):
                        continue

                node_data[name] = {"devices": valid}
                per_node_status[name] = {
                    "ok": True, "devices": len(valid), "url": base_url, "last_success": time.time(),
                }
                total_probes += len(valid)
            except Exception as exc:
                per_node_status[name] = {
                    "ok": False, "devices": 0, "url": base_url,
                    "error": str(exc), "last_success": None,
                }

        return node_data, per_node_status, total_probes

    def _update_mode_from_claps(self) -> None:
        """Advance the display mode when the mic node's clap counter changes."""
        clap_url = config.NODES.get(config.CLAP_NODE)
        if not clap_url:
            return
        try:
            resp = requests.get(f"{clap_url}/api/status", timeout=ACTUATOR_TIMEOUT_S)
            resp.raise_for_status()
            clap_count = resp.json().get("clap_count")
        except Exception as exc:
            print(f"[CLAP CHECK FAILED - {config.CLAP_NODE}]", exc)
            return

        if clap_count is None:
            return
        if self._last_clap_count is None:
            self._last_clap_count = clap_count  # baseline; don't trigger on startup
        elif clap_count != self._last_clap_count:
            idx = config.MODE_CYCLE.index(self.display_mode)
            self.display_mode = config.MODE_CYCLE[(idx + 1) % len(config.MODE_CYCLE)]
            self._last_clap_count = clap_count
            print(f"[MODE] Clap on {config.CLAP_NODE} -> mode is now {self.display_mode}")

    def _push_display(self, per_node_status, node_counts, room_count) -> None:
        """Send each node the payload for the current mode."""
        max_local = max(node_counts) if node_counts else 0

        for name, base_url in config.NODES.items():
            if self.display_mode == config.MODE_COMBINED:
                payload = {"mode": config.MODE_COMBINED, "count": room_count}
            elif self.display_mode == config.MODE_AREA:
                local = per_node_status.get(name, {}).get("devices", 0)
                scale = (local / max_local) if max_local > 0 else 0.0
                payload = {"mode": config.MODE_AREA, "count": local, "scale": round(scale, 2)}
            else:
                payload = {"mode": config.MODE_STANDBY}

            try:
                requests.post(f"{base_url}/api/actuator", json=payload, timeout=ACTUATOR_TIMEOUT_S)
            except Exception as exc:
                print(f"[DISPLAY PUSH FAILED - {name}]", exc)
