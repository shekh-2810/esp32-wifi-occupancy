#pragma once

// HTTP API served by every node:
//   GET  /                 landing page
//   GET  /api/status       node health + sniffer counters (+ clap_count on mic role)
//   GET  /api/devices      hashed device list (polled by the server)
//   POST /api/actuator     {"mode": "...", "count": N, "scale": 0..1}
namespace api {

void begin();          // register routes and start listening (needs Wi-Fi up)
bool started();
void handleClient();   // call every loop()

}  // namespace api
