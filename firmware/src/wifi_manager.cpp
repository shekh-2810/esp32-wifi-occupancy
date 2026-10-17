#include "wifi_manager.h"

#include <WiFi.h>

#include "config.h"

namespace wifi_manager {
namespace {

uint32_t lastRetryMs = 0;

void applyStaticIp() {
    IPAddress ip(NET_PREFIX_A, NET_PREFIX_B, NET_PREFIX_C, STATIC_IP_LAST_OCTET);
    IPAddress gateway(NET_PREFIX_A, NET_PREFIX_B, NET_PREFIX_C, NET_GATEWAY_LAST_OCTET);
    IPAddress subnet(255, 255, 255, 0);
    if (!WiFi.config(ip, gateway, subnet, gateway)) {
        Serial.println("WiFi.config() failed -- falling back to DHCP.");
    }
}

bool waitForConnection(uint32_t timeoutMs, uint32_t pollMs, bool printDots) {
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(pollMs);
        if (printDots) Serial.print(".");
    }
    if (printDots) Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

}  // namespace

bool connect() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // keep the radio awake for reliable sniffing
    applyStaticIp();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    if (!waitForConnection(WIFI_CONNECT_TIMEOUT_MS, 250, true)) {
        Serial.println("WiFi FAILED.");
        lastRetryMs = millis();
        return false;
    }
    Serial.print("WiFi connected. IP: ");  Serial.println(WiFi.localIP());
    Serial.print("AP channel: ");          Serial.println(WiFi.channel());
    return true;
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

bool maintain() {
    if (isConnected()) return false;

    uint32_t now = millis();
    if (now - lastRetryMs < WIFI_RETRY_INTERVAL_MS) return false;
    lastRetryMs = now;

    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    applyStaticIp();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    if (waitForConnection(WIFI_CONNECT_TIMEOUT_MS, 100, false)) {
        Serial.print("Reconnected. IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }
    return false;
}

}  // namespace wifi_manager
