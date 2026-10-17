#pragma once
#include <Arduino.h>

// Mode-change announcer (buzzer role only).
namespace buzzer {

void begin();
void update();                              // call every loop()
void announceMode(const String& mode);      // plays a tune when the mode changes

}  // namespace buzzer
