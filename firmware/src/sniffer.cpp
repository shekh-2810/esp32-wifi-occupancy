#include "sniffer.h"

#include <esp_wifi.h>

#include "config.h"

namespace sniffer {
namespace {

struct SeenDevice {
    uint32_t id = 0;
    int8_t   lastRssi = 0;
    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs = 0;
    uint32_t probeCount = 0;
    bool     active = false;
};

SeenDevice devices[MAX_DEVICES_TRACKED];

uint32_t windowProbeCount = 0;
uint32_t windowNewDevices = 0;
uint32_t totalUnique = 0;
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

// FNV-1a over the MAC, salted. The raw MAC is never stored or transmitted.
uint32_t hashMac(const uint8_t* mac) {
    uint32_t hash = 2166136261u ^ HASH_SALT;
    for (int i = 0; i < 6; i++) {
        hash ^= mac[i];
        hash *= 16777619u;
    }
    return hash;
}

void recordDevice(uint32_t id, int8_t rssi, uint32_t now) {
    for (int i = 0; i < MAX_DEVICES_TRACKED; i++) {
        if (devices[i].active && devices[i].id == id) {
            devices[i].lastRssi = rssi;
            devices[i].lastSeenMs = now;
            devices[i].probeCount++;
            return;
        }
    }
    for (int i = 0; i < MAX_DEVICES_TRACKED; i++) {
        if (!devices[i].active) {
            devices[i].id = id;
            devices[i].lastRssi = rssi;
            devices[i].firstSeenMs = now;
            devices[i].lastSeenMs = now;
            devices[i].probeCount = 1;
            devices[i].active = true;
            windowNewDevices++;
            totalUnique++;
            return;
        }
    }
    // Table full: new devices are dropped until old ones time out.
}

// Caller must hold dataMux.
void pruneStale(uint32_t now) {
    for (int i = 0; i < MAX_DEVICES_TRACKED; i++) {
        if (devices[i].active && (now - devices[i].lastSeenMs > DEVICE_TIMEOUT_MS)) {
            devices[i].active = false;
        }
    }
}

void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    auto* packet = reinterpret_cast<wifi_promiscuous_pkt_t*>(buf);
    const uint8_t* payload = packet->payload;

    // 0x40 = management frame, subtype probe request
    if ((payload[0] & 0xF0) != 0x40) return;

    const uint8_t* sourceMac = payload + 10;
    int8_t   rssi = packet->rx_ctrl.rssi;
    uint32_t id   = hashMac(sourceMac);
    uint32_t now  = millis();

    portENTER_CRITICAL_ISR(&dataMux);
    recordDevice(id, rssi, now);
    windowProbeCount++;
    portEXIT_CRITICAL_ISR(&dataMux);
}

}  // namespace

void enable() {
    // Idempotent: also registers the callback, so a node that missed Wi-Fi at
    // boot still starts sniffing once it reconnects.
    esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
    esp_wifi_set_promiscuous(true);
}

void begin() {
    enable();
    Serial.println("Sniffer active.");
}

int activeCount() {
    uint32_t now = millis();
    int active = 0;
    portENTER_CRITICAL(&dataMux);
    pruneStale(now);
    for (int i = 0; i < MAX_DEVICES_TRACKED; i++) {
        if (devices[i].active) active++;
    }
    portEXIT_CRITICAL(&dataMux);
    return active;
}

uint32_t totalUniqueSinceBoot() { return totalUnique; }

String statusFields() {
    uint32_t now = millis();
    int  active = 0;
    long rssiSum = 0;

    portENTER_CRITICAL(&dataMux);
    pruneStale(now);
    for (int i = 0; i < MAX_DEVICES_TRACKED; i++) {
        if (devices[i].active) {
            active++;
            rssiSum += devices[i].lastRssi;
        }
    }
    uint32_t probes = windowProbeCount;
    uint32_t newDev = windowNewDevices;
    windowProbeCount = 0;
    windowNewDevices = 0;
    portEXIT_CRITICAL(&dataMux);

    double avgRssi = active ? (double)rssiSum / active : 0.0;

    String json;
    json += "\"devices\":" + String(active) + ",";
    json += "\"new_devices\":" + String(newDev) + ",";
    json += "\"total_unique_since_boot\":" + String(totalUnique) + ",";
    json += "\"probes\":" + String(probes) + ",";
    json += "\"avg_rssi\":" + String(avgRssi, 2);
    return json;
}

String devicesJson(const String& nodeId) {
    uint32_t now = millis();
    String json = "{\"node\":\"" + nodeId + "\",\"now_ms\":" + String(now) + ",\"devices\":[";

    portENTER_CRITICAL(&dataMux);
    pruneStale(now);
    bool first = true;
    for (int i = 0; i < MAX_DEVICES_TRACKED; i++) {
        if (!devices[i].active) continue;
        if (!first) json += ",";
        first = false;
        json += "{\"id\":" + String(devices[i].id) + ",";
        json += "\"rssi\":" + String(devices[i].lastRssi) + ",";
        json += "\"last_seen_ms\":" + String(devices[i].lastSeenMs) + ",";
        json += "\"probe_count\":" + String(devices[i].probeCount) + "}";
    }
    portEXIT_CRITICAL(&dataMux);

    json += "]}";
    return json;
}

}  // namespace sniffer
