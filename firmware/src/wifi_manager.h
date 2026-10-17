#pragma once

namespace wifi_manager {

bool connect();                 // blocking; returns true when connected
bool isConnected();
// Call every loop(). Retries a dropped connection; returns true on the loop
// iteration where a reconnect just succeeded.
bool maintain();

}  // namespace wifi_manager
