#include "buzzer.h"

#include "config.h"

#ifdef ROLE_BUZZER

#include "esp_arduino_version.h"

namespace buzzer {
namespace {

String   lastAnnouncedMode = "";
uint32_t stopAtMs = 0;
bool     active = false;

void toneRaw(uint32_t freqHz) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWriteTone(BUZZER_PIN, freqHz);
#else
    ledcWriteTone(0, freqHz);
#endif
}

void playTone(uint32_t freqHz, uint32_t durationMs) {
    toneRaw(freqHz);
    active = true;
    stopAtMs = millis() + durationMs;
}

}  // namespace

void begin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(BUZZER_PIN, 2000, 10);
    ledcWrite(BUZZER_PIN, 0);
#else
    ledcSetup(0, 2000, 10);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 0);
#endif
}

void update() {
    if (active && millis() >= stopAtMs) {
        toneRaw(0);
        active = false;
    }
}

// This board has no mic: it announces the mode the SERVER settled on, so the
// tune always reflects the real current mode.
void announceMode(const String& mode) {
    if (mode == lastAnnouncedMode) return;
    lastAnnouncedMode = mode;

    if (mode == "combined")     playTone(1000, 150);  // single steady beep
    else if (mode == "area")    playTone(1400, 90);   // quick beep
    else if (mode == "standby") playTone(600, 250);   // low, longer beep

    Serial.printf("[BUZZER] Mode announced: %s\n", mode.c_str());
}

}  // namespace buzzer

#else  // not the buzzer role: stubs

namespace buzzer {
void begin() {}
void update() {}
void announceMode(const String&) {}
}  // namespace buzzer

#endif
