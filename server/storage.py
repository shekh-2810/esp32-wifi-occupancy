"""SQLite persistence for diagnostic history (no ground-truth labels)."""
import json
import sqlite3
import threading

import config

_db_lock = threading.Lock()

_SCHEMA = """
CREATE TABLE IF NOT EXISTS history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts REAL NOT NULL,
    raw_count INTEGER,
    confirmed_count INTEGER,
    candidate_count INTEGER,
    node_counts TEXT,
    mean_rssi REAL,
    rssi_stddev REAL,
    probe_rate INTEGER,
    confidence REAL,
    node_agreement REAL
)
"""


def init_db() -> None:
    config.DATA_DIR.mkdir(parents=True, exist_ok=True)
    with _db_lock:
        conn = sqlite3.connect(config.DB_PATH)
        try:
            conn.execute(_SCHEMA)
            conn.commit()
        finally:
            conn.close()


def persist_row(ts, raw_count, confirmed_count, candidate_count, node_counts,
                mean_rssi, rssi_stddev, probe_rate, confidence, node_agreement) -> None:
    with _db_lock:
        conn = sqlite3.connect(config.DB_PATH)
        try:
            conn.execute(
                """
                INSERT INTO history (
                    ts, raw_count, confirmed_count, candidate_count, node_counts,
                    mean_rssi, rssi_stddev, probe_rate, confidence, node_agreement
                ) VALUES (?,?,?,?,?,?,?,?,?,?)
                """,
                (ts, raw_count, confirmed_count, candidate_count, json.dumps(node_counts),
                 mean_rssi, rssi_stddev, probe_rate, confidence, node_agreement),
            )
            conn.commit()
        finally:
            conn.close()
