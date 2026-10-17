/*
 * ESP32 Wi-Fi occupancy node
 * --------------------------
 * Every node passively sniffs Wi-Fi probe requests (hashed device IDs only),
 * serves them over HTTP, and renders whatever display mode the Python server
 * sends to its 8-pixel NeoPixel strip.
 *
 * Board role is chosen at build time (see platformio.ini):
 *   node1_mic     + HW-484 microphone: counts double-claps (mode switch)
 *   node2_buzzer  + buzzer: beeps when the display mode changes
 *   node3_display plain sniffer + NeoPixel
 */

#include <Arduino.h>

#include "api.h"
#include "buzzer.h"
#include "clap.h"
#include "config.h"
#include "display.h"
#include "sniffer.h"
#include "wifi_manager.h"

namespace {

uint32_t lastReportMs = 0;
uint32_t lastBlinkToggleMs = 0;
bool     ledOn = false;

// Heartbeat blink when healthy, fast blink when Wi-Fi is down.
void updateStatusLed() {
    uint32_t now = millis();
    bool healthy = wifi_manager::isConnected();
    uint32_t onMs  = healthy ? HEARTBEAT_ON_MS : ERROR_BLINK_MS;
    uint32_t period = healthy ? HEARTBEAT_INTERVAL_MS : (ERROR_BLINK_MS * 2);

    if (ledOn) {
        if (now - lastBlinkToggleMs >= onMs) {
            digitalWrite(LED_PIN, LOW);
            ledOn = false;
            lastBlinkToggleMs = now;
        }
    } else if (now - lastBlinkToggleMs >= (period - onMs)) {
        digitalWrite(LED_PIN, HIGH);
        ledOn = true;
        lastBlinkToggleMs = now;
    }
}

void reportToSerial() {
    Serial.printf("[%s] active devices: %d | total unique: %u | heap: %u | mode: %s",
                  NODE_ID, sniffer::activeCount(), sniffer::totalUniqueSinceBoot(),
                  ESP.getFreeHeap(), display::mode().c_str());
#ifdef ROLE_MIC
    Serial.printf(" | claps: %u", clap::count());
#endif
    Serial.println();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.printf(" %s | static IP target: %d.%d.%d.%d\n", NODE_ID,
                  NET_PREFIX_A, NET_PREFIX_B, NET_PREFIX_C, STATIC_IP_LAST_OCTET);
    Serial.println("==========================================");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    clap::begin();    // no-ops on boards without that hardware
    buzzer::begin();
    display::begin();

    if (wifi_manager::connect()) {
        api::begin();
        sniffer::begin();
    }

    lastReportMs = millis();
    lastBlinkToggleMs = millis();
    Serial.println("Init complete.");
}

void loop() {
    api::handleClient();

    if (wifi_manager::maintain()) {  // just reconnected
        if (!api::started()) api::begin();
        sniffer::enable();
    }

    uint32_t now = millis();
    if (now - lastReportMs >= REPORT_INTERVAL_MS) {
        reportToSerial();
        lastReportMs = now;
    }

    updateStatusLed();
    clap::update();
    buzzer::update();
    display::update();
    yield();
}
