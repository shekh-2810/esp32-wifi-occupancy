"""Server configuration. Every value can be overridden with an environment variable."""
import os
from pathlib import Path

# ---- Nodes ----------------------------------------------------------------
# Override with: OCC_NODES="NODE-01=http://192.168.137.203,NODE-02=http://...,..."
_DEFAULT_NODES = {
    "NODE-01": "http://192.168.137.203",  # mic board
    "NODE-02": "http://192.168.137.202",  # buzzer board
    "NODE-03": "http://192.168.137.201",  # display-only board
}


def _parse_nodes(raw: str) -> dict:
    nodes = {}
    for item in raw.split(","):
        if "=" in item:
            name, url = item.split("=", 1)
            nodes[name.strip()] = url.strip().rstrip("/")
    return nodes or dict(_DEFAULT_NODES)


NODES = _parse_nodes(os.getenv("OCC_NODES", ""))

# The node with the microphone; its clap_count drives display-mode changes.
CLAP_NODE = os.getenv("OCC_CLAP_NODE", "NODE-01")

# ---- Display modes (cycled by a double-clap) -------------------------------
MODE_COMBINED = "combined"  # every strip shows the fused room count
MODE_AREA = "area"          # each strip shows its own node's count, brightness relative to busiest
MODE_STANDBY = "standby"    # steady light, ignores occupancy
MODE_CYCLE = [MODE_COMBINED, MODE_AREA, MODE_STANDBY]

# ---- Polling ----------------------------------------------------------------
POLL_INTERVAL_S = float(os.getenv("OCC_POLL_INTERVAL_S", "2"))
STALE_AFTER_MS = 30_000     # ignore devices a node last saw longer ago than this
HISTORY_SIZE = 1800         # in-memory samples kept for the live chart

# ---- Noise filters (rule-based, no training data) ---------------------------
# Devices whose strongest RSSI is weaker than this are treated as bleed-through
# from outside the room. Tune by standing at the doorway and watching the count.
RSSI_PRESENCE_THRESHOLD = int(os.getenv("OCC_RSSI_THRESHOLD", "-80"))  # dBm
CONFIRM_AFTER_POLLS = 2        # consecutive polls before a device counts
DROP_AFTER_MISSED_POLLS = 2    # consecutive missed polls before it is dropped

# ---- Storage / web ----------------------------------------------------------
DATA_DIR = Path(os.getenv("OCC_DATA_DIR", Path(__file__).parent / "data"))
DB_PATH = DATA_DIR / "occupancy_history.db"
HOST = os.getenv("OCC_HOST", "0.0.0.0")
PORT = int(os.getenv("OCC_PORT", "8000"))
