"""Occupancy dashboard server.

Run:
    pip install -r requirements.txt
    python app.py
Then open http://localhost:8000
"""
import threading

from flask import Flask, jsonify, render_template

import config
import storage
from engine import OccupancyEngine

app = Flask(__name__)
engine = OccupancyEngine()


@app.route("/")
def index():
    return render_template("dashboard.html")


@app.route("/api/fused")
def api_fused():
    return jsonify(engine.snapshot())


@app.route("/api/history")
def api_history():
    return jsonify({"history": engine.full_history()})


def main() -> None:
    print("=" * 60)
    print(" ESP32 WI-FI OCCUPANCY SERVER")
    print("=" * 60)
    for name, url in config.NODES.items():
        print(f"  {name:<10} {url}")
    print(f"Database : {config.DB_PATH}")
    print(f"Dashboard: http://localhost:{config.PORT}")
    print(f"Tuning   : RSSI_PRESENCE_THRESHOLD={config.RSSI_PRESENCE_THRESHOLD}, "
          f"CONFIRM_AFTER_POLLS={config.CONFIRM_AFTER_POLLS}, "
          f"DROP_AFTER_MISSED_POLLS={config.DROP_AFTER_MISSED_POLLS}")
    print()

    storage.init_db()
    threading.Thread(target=engine.run_forever, daemon=True).start()
    app.run(host=config.HOST, port=config.PORT, threaded=True)


if __name__ == "__main__":
    main()
