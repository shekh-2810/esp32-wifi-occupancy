#include "clap.h"

#include "config.h"

#ifdef ROLE_MIC

namespace clap {
namespace {

uint32_t eventCount = 0;
int      lastAnalogValue = 0;
bool     lastMicState = LOW;
uint32_t lastPulseMs = 0;
uint32_t firstClapMs = 0;
bool     clapPending = false;

}  // namespace

void begin() {
    pinMode(MIC_DO_PIN, INPUT);
    pinMode(MIC_AO_PIN, INPUT);
    analogReadResolution(12);
}

void update() {
    uint32_t now = millis();

    // DO gives the timing pulse (assumed active-HIGH); AO filters weak noise.
    int digitalState = digitalRead(MIC_DO_PIN);
    int analogValue  = analogRead(MIC_AO_PIN);
    lastAnalogValue  = analogValue;

    bool risingEdge = (digitalState == HIGH && lastMicState == LOW);

    if (risingEdge &&
        analogValue >= CLAP_ANALOG_THRESHOLD &&
        (now - lastPulseMs) >= CLAP_REFRACTORY_MS) {

        lastPulseMs = now;
        Serial.printf("[MIC] Pulse: DO=%d AO=%d\n", digitalState, analogValue);

        if (!clapPending) {
            clapPending = true;
            firstClapMs = now;
            Serial.println("[MIC] First clap detected");
        } else {
            uint32_t gap = now - firstClapMs;
            if (gap >= CLAP_WINDOW_MIN_MS && gap <= CLAP_WINDOW_MAX_MS) {
                eventCount++;
                Serial.printf("[MIC] DOUBLE-CLAP #%u | gap=%lu ms | AO=%d\n",
                              eventCount, (unsigned long)gap, analogValue);
            } else {
                Serial.printf("[MIC] Second pulse outside window: %lu ms\n", (unsigned long)gap);
            }
            clapPending = false;
        }
    }

    lastMicState = digitalState;

    // First clap never got a partner.
    if (clapPending && (now - firstClapMs) > CLAP_WINDOW_MAX_MS) {
        Serial.println("[MIC] Double-clap timeout");
        clapPending = false;
    }
}

uint32_t count()       { return eventCount; }
int      lastAnalog()  { return lastAnalogValue; }
int      digitalLevel(){ return digitalRead(MIC_DO_PIN); }

}  // namespace clap

#else  // not the mic role: harmless stubs so callers need no #ifdefs

namespace clap {
void begin() {}
void update() {}
uint32_t count() { return 0; }
int lastAnalog() { return 0; }
int digitalLevel() { return 0; }
}  // namespace clap

#endif
