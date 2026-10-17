#pragma once
#include <Arduino.h>

// 8-pixel NeoPixel renderer. The server decides what to show; the node just
// renders the last payload it received.
namespace display {

void begin();
void setState(const String& mode, int count, float scale);  // from POST /api/actuator
void update();                                              // call every loop()
const String& mode();

}  // namespace display
