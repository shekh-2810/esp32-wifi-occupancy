#pragma once
#include <Arduino.h>

// Double-clap detector (mic role only). It only COUNTS claps; the server notices
// the count change via /api/status and decides the display mode.
namespace clap {

void begin();
void update();                 // call every loop()
uint32_t count();              // confirmed double-claps since boot
int lastAnalog();              // last AO reading
int digitalLevel();            // current DO level

}  // namespace clap
