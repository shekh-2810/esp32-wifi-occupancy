# esp32-wifi-occupancy

Estimate how many people are in a room using **three ESP32 boards** that passively listen for Wi-Fi probe requests, plus a small **Flask server** that fuses their data, filters noise, and shows a live dashboard. Each board also has an 8-pixel NeoPixel strip that displays the result, and a double-clap switches the display mode.

> **Status:** hobby / learning project. The count is a *device* estimate, not a verified head count.

## How it works

```
 Phones nearby ──probe requests──▶  ESP32 nodes (x3, promiscuous mode)
                                         │  hashed device IDs + RSSI
                                         ▼   GET /api/devices   (every 2 s)
                                   Flask server
                      fuse nodes → RSSI gate → confirm/drop hysteresis
                                         │
                       ┌─────────────────┼──────────────────┐
                       ▼                 ▼                  ▼
                  Dashboard         SQLite history     POST /api/actuator
                (live chart)                            (NeoPixel mode/count)
```

1. **Sniff** – every node captures Wi-Fi *probe request* frames and stores only a **salted hash** of the source MAC, with the latest RSSI.
2. **Fuse** – the server polls all nodes and merges devices by hashed ID (same salt on every node ⇒ same device, same ID), keeping the strongest RSSI.
3. **Denoise** (rule-based, no training data):
   - **RSSI gate** – devices weaker than `RSSI_PRESENCE_THRESHOLD` (default −80 dBm) are treated as bleed-through from outside the room.
   - **Hysteresis** – a device must be seen `CONFIRM_AFTER_POLLS` polls in a row to count, and missed `DROP_AFTER_MISSED_POLLS` polls in a row to be dropped.
4. **Display** – the server pushes the result to each board's LED strip. Modes (cycled by a double-clap on the mic board):
   - `combined` – every strip shows the fused room count
   - `area` – each strip shows its own node's count; brightness is relative to the busiest node
   - `standby` – steady light, ignores occupancy

   If a board hears nothing from the server for 60 s, its strip shows a slow amber "breathing" animation instead of a stale number.

## Hardware

| Role | Board | Extra hardware | Static IP |
|------|-------|----------------|-----------|
| `node1_mic` (NODE-01) | ESP32 dev board | HW-484 microphone module, NeoPixel strip | `.203` |
| `node2_buzzer` (NODE-02) | ESP32 dev board | Buzzer, NeoPixel strip | `.202` |
| `node3_display` (NODE-03) | ESP32 dev board | NeoPixel strip | `.201` |

| Signal | GPIO |
|--------|------|
| NeoPixel data (8 pixels) | 4 |
| Mic digital out (DO) | 34 |
| Mic analog out (AO) | 35 |
| Buzzer | 16 |
| Status LED | built-in (2) |

Pins and timings live in [`firmware/include/config.h`](firmware/include/config.h).

## Repository layout

```
firmware/                 PlatformIO project (one codebase, three roles)
  platformio.ini            envs: node1_mic, node2_buzzer, node3_display
  include/config.h          pins, timings, network layout
  include/secrets.example.h copy to secrets.h (gitignored)
  src/                      main, sniffer, display, clap, buzzer, api, wifi_manager
server/                   Flask app
  app.py                    routes + startup
  engine.py                 polling loop, clap → mode, display push
  fusion.py                 fusion, presence tracker, confidence, jump detection
  storage.py                SQLite history
  config.py                 settings (env-var overridable)
  templates/dashboard.html  live dashboard
```

## Quick start

### 1. Network

The defaults assume the nodes and the server share a network in `192.168.137.0/24` (the Windows *Mobile Hotspot* default, gateway `192.168.137.1`). If yours differs, edit `NET_PREFIX_*` in `firmware/include/config.h` and the node URLs in `server/config.py` (or set `OCC_NODES`).

### 2. Flash the nodes

Requires [PlatformIO](https://platformio.org/).

```bash
cd firmware
cp include/secrets.example.h include/secrets.h   # then edit it
```

In `secrets.h` set your Wi-Fi SSID/password and choose your own random `HASH_SALT` (**identical on all three boards**). Then flash each board with its role:

```bash
pio run -e node1_mic     -t upload   # board with the microphone
pio run -e node2_buzzer  -t upload   # board with the buzzer
pio run -e node3_display -t upload   # display-only board
pio device monitor                   # 115200 baud; confirm IP and "Sniffer active."
```

### 3. Run the server

```bash
cd server
python -m venv .venv && source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python app.py
```

Open <http://localhost:8000>.

## Configuration

Server settings are in [`server/config.py`](server/config.py) and can be overridden with environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `OCC_NODES` | the three `192.168.137.20x` nodes | `NODE-01=http://ip,NODE-02=http://ip,...` |
| `OCC_CLAP_NODE` | `NODE-01` | Node whose `clap_count` switches display mode |
| `OCC_RSSI_THRESHOLD` | `-80` | Presence gate in dBm. Stand just outside the door and tune until the count drops |
| `OCC_POLL_INTERVAL_S` | `2` | Seconds between polls |
| `OCC_DATA_DIR` | `server/data` | Where the SQLite history is stored |
| `OCC_HOST` / `OCC_PORT` | `0.0.0.0` / `8000` | Listen address |

Clap sensitivity is `CLAP_ANALOG_THRESHOLD` in `config.h`; watch `mic_ao` in `/api/status` while clapping to choose a value.

## HTTP API

**Node** (port 80)

| Endpoint | Description |
|----------|-------------|
| `GET /api/status` | Health, sniffer counters, display mode (+ `clap_count`, `mic_ao`, `mic_do` on the mic board) |
| `GET /api/devices` | Currently active hashed devices with RSSI |
| `POST /api/actuator` | `{"mode":"combined","count":N}` · `{"mode":"area","count":N,"scale":0..1}` · `{"mode":"standby"}` |

> `GET /api/status` resets the per-window `probes` / `new_devices` counters.

**Server** (port 8000): `GET /api/fused` (latest state + recent history), `GET /api/history` (in-memory history).

## Privacy and legal

- Only **salted hashes** of MAC addresses are kept, and nothing leaves your local network. Still, a salted hash of a 48-bit MAC is pseudonymous, not anonymous, so keep `secrets.h` and the SQLite files private (both are gitignored).
- Many phones randomize their MAC in probe requests, so one person can appear as several devices, and some devices never probe. The count is an estimate.
- Passive sniffing and presence tracking may be regulated where you live (e.g. GDPR). Use it on your own premises, tell the people affected, and don't deploy it in public or shared spaces without checking the rules.

## Known limitations

- Estimates *devices*, not people (one person may carry several devices, or none).
- Each node tracks at most 128 devices at once.
- Plain HTTP with no authentication, so run it on a trusted network only.
- Dashboard loads Chart.js from a CDN, so it needs internet access on the viewing machine.

## License

[MIT](LICENSE)
