"""Pure, rule-based signal processing: fusion, presence confirmation, scoring.

Pipeline per poll:
    per-node device lists -> fuse_devices -> PresenceTracker (RSSI gate +
    confirm/drop hysteresis) -> confidence / unusual-jump flag
"""
import statistics

import config


def clamp(value, low, high):
    return max(low, min(high, value))


def fuse_devices(node_data: dict) -> list:
    """Merge devices seen by several nodes.

    Node firmware hashes MACs with a shared salt, so the same physical device
    has the same ID on every node. Keeps the strongest RSSI and which nodes
    saw it.
    """
    merged = {}
    for node_name, data in node_data.items():
        for dev in data.get("devices", []):
            try:
                device_id = int(dev["id"])
                rssi = int(dev["rssi"])
            except (KeyError, TypeError, ValueError):
                continue

            entry = merged.setdefault(device_id, {"id": device_id, "rssi": rssi, "seen_by": []})
            entry["rssi"] = max(entry["rssi"], rssi)
            if node_name not in entry["seen_by"]:
                entry["seen_by"].append(node_name)
    return list(merged.values())


def node_agreement(node_counts: list) -> float:
    """0..1 -- how closely the nodes' individual counts agree."""
    if not node_counts:
        return 0.0
    mean_count = statistics.mean(node_counts)
    if mean_count <= 0:
        return 1.0
    deviation = statistics.mean(abs(x - mean_count) for x in node_counts)
    return 1.0 - clamp(deviation / max(mean_count, 1), 0, 1)


def calculate_confidence(active_nodes, total_nodes, fused_devices, node_counts, rssi_values) -> float:
    """Sensor-quality score (0-100). NOT the probability that the count is right.

    Weights: node availability 35%, node agreement 30%, multi-node
    observation ratio 20%, RSSI quality 15%.
    """
    if total_nodes == 0:
        return 0

    availability = active_nodes / total_nodes
    agreement = node_agreement(node_counts)

    if rssi_values:
        # -90 dBm is weak, -40 dBm is strong
        rssi_quality = clamp((statistics.mean(rssi_values) + 90) / 50, 0, 1)
    else:
        rssi_quality = 0

    multi_node_ratio = 0
    if fused_devices:
        multi_node_ratio = sum(len(d["seen_by"]) > 1 for d in fused_devices) / len(fused_devices)

    confidence = (availability * 0.35 + agreement * 0.30 +
                  rssi_quality * 0.15 + multi_node_ratio * 0.20)
    return round(confidence * 100, 1)


class PresenceTracker:
    """RSSI proximity gate + confirm/drop hysteresis.

    1. Devices weaker than RSSI_PRESENCE_THRESHOLD are ignored (bleed-through).
    2. A device must be seen CONFIRM_AFTER_POLLS polls in a row to count.
    3. A device must be missing DROP_AFTER_MISSED_POLLS polls in a row to be
       dropped, which absorbs a single failed poll without flicker.
    """

    def __init__(self):
        self.states = {}

    def update(self, fused_devices: list):
        seen = {d["id"]: d for d in fused_devices if d["rssi"] >= config.RSSI_PRESENCE_THRESHOLD}

        for device_id, dev in seen.items():
            state = self.states.setdefault(
                device_id, {"seen_streak": 0, "missed_streak": 0, "confirmed": False})
            state["seen_streak"] += 1
            state["missed_streak"] = 0
            state["last_rssi"] = dev["rssi"]
            state["seen_by"] = dev["seen_by"]
            if not state["confirmed"] and state["seen_streak"] >= config.CONFIRM_AFTER_POLLS:
                state["confirmed"] = True

        for device_id in list(self.states):
            if device_id not in seen:
                state = self.states[device_id]
                state["seen_streak"] = 0
                state["missed_streak"] += 1
                if state["missed_streak"] >= config.DROP_AFTER_MISSED_POLLS:
                    del self.states[device_id]

        confirmed = sum(1 for s in self.states.values() if s["confirmed"])
        candidates = len(self.states) - confirmed
        return confirmed, candidates


def detect_unusual_jump(history, current):
    """Flag when `current` is >= 3 sigma from the last 20 samples. Returns (flag, z)."""
    if len(history) < 20:
        return False, 0

    values = [x["occupancy"] for x in list(history)[-20:]]
    mean = statistics.mean(values)
    std = max(statistics.stdev(values), 1)
    z = abs(current - mean) / std
    return z >= 3, round(z, 2)
