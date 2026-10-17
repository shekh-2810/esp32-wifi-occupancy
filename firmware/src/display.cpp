#include "display.h"

#include <Adafruit_NeoPixel.h>
#include <math.h>

#include "config.h"

namespace display {
namespace {

Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

String   currentMode = "standby";
int      currentCount = 0;
float    currentScale = 1.0f;
uint32_t lastUpdateMs = 0;
bool     everUpdated = false;

uint32_t colorForFraction(float fraction) {
    if (fraction <= 0.4f)  return strip.Color(40, 180, 40);   // green
    if (fraction <= 0.75f) return strip.Color(200, 140, 0);   // amber
    return strip.Color(200, 30, 30);                          // red
}

void renderStandby() {
    uint32_t warmWhite = strip.Color(120, 90, 40);
    strip.setBrightness(90);
    for (int i = 0; i < NEOPIXEL_COUNT; i++) strip.setPixelColor(i, warmWhite);
    strip.show();
}

// brightnessScale: 1.0 = full. "combined" mode always passes 1.0; "area" mode
// passes the server-computed busyness relative to the busiest node.
void renderBarGraph(int count, float brightnessScale) {
    int lit = constrain(count, 0, NEOPIXEL_COUNT);
    uint32_t color = colorForFraction((float)lit / NEOPIXEL_COUNT);
    strip.setBrightness((uint8_t)constrain(40 + brightnessScale * 215, 0, 255));
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        strip.setPixelColor(i, i < lit ? color : strip.Color(0, 0, 0));
    }
    strip.show();
}

// Slow amber "breathing" when the server has gone quiet.
void renderStale() {
    float phase  = (millis() % 3000) / 3000.0f;
    float breath = (sinf(phase * 2 * PI) + 1.0f) / 2.0f;
    strip.setBrightness(255);
    uint8_t r = (uint8_t)(120 * breath);
    uint8_t g = (uint8_t)(70 * breath);
    for (int i = 0; i < NEOPIXEL_COUNT; i++) strip.setPixelColor(i, strip.Color(r, g, 0));
    strip.show();
}

}  // namespace

void begin() {
    strip.begin();
    renderStandby();
}

void setState(const String& mode, int count, float scale) {
    currentMode = mode;
    currentCount = count;
    currentScale = scale;
    lastUpdateMs = millis();
    everUpdated = true;
}

const String& mode() { return currentMode; }

void update() {
    bool stale = everUpdated && (millis() - lastUpdateMs > ACTUATOR_STALE_MS);

    if (stale)                        renderStale();
    else if (currentMode == "standby") renderStandby();
    else if (currentMode == "area")    renderBarGraph(currentCount, currentScale);
    else                               renderBarGraph(currentCount, 1.0f);  // "combined" / unknown
}

}  // namespace display
