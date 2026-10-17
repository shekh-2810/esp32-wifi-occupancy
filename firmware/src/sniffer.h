#pragma once
#include <Arduino.h>

// Passive Wi-Fi probe-request sniffer. Tracks hashed device IDs only.
namespace sniffer {

void begin();                       // register callback and enable promiscuous mode
void enable();                      // re-enable promiscuous mode (e.g. after reconnect)

int      activeCount();             // devices seen within DEVICE_TIMEOUT_MS
uint32_t totalUniqueSinceBoot();

// JSON fragments (no surrounding braces) / documents used by the HTTP API.
// NOTE: statusFields() resets the per-window counters (probes, new_devices).
String statusFields();
String devicesJson(const String& nodeId);

}  // namespace sniffer
