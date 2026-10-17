#pragma once
#include <Arduino.h>
#include "secrets.h"

// ---- Role / identity (normally injected by platformio.ini build_flags) ----
#if !defined(ROLE_MIC) && !defined(ROLE_BUZZER) && !defined(ROLE_DISPLAY)
#define ROLE_DISPLAY
#endif
#ifndef NODE_ID
#define NODE_ID "NODE-03"
#endif
#ifndef STATIC_IP_LAST_OCTET
#define STATIC_IP_LAST_OCTET 201
#endif

// ---- Network (192.168.137.x is the Windows "Mobile hotspot" default) ----
#define NET_PREFIX_A 192
#define NET_PREFIX_B 168
#define NET_PREFIX_C 137
#define NET_GATEWAY_LAST_OCTET 1

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS  = 10000;

// ---- Sniffer ----
constexpr uint32_t DEVICE_TIMEOUT_MS   = 30000;  // forget a device after this long
constexpr int      MAX_DEVICES_TRACKED = 128;

// ---- Status LED ----
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
constexpr int      LED_PIN               = LED_BUILTIN;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 1000;
constexpr uint32_t HEARTBEAT_ON_MS       = 50;
constexpr uint32_t ERROR_BLINK_MS        = 120;

// ---- NeoPixel strip ----
constexpr int      NEOPIXEL_PIN      = 4;
constexpr int      NEOPIXEL_COUNT    = 8;
constexpr uint32_t ACTUATOR_STALE_MS = 60000;  // show "no data" animation after this

// ---- Serial report ----
constexpr uint32_t REPORT_INTERVAL_MS = 5000;

// ---- Mic role (HW-484 module) ----
constexpr int      MIC_DO_PIN             = 34;   // digital comparator output
constexpr int      MIC_AO_PIN             = 35;   // analog output
constexpr int      CLAP_ANALOG_THRESHOLD  = 150;  // tune by watching mic_ao on Serial
constexpr uint32_t CLAP_REFRACTORY_MS     = 60;
constexpr uint32_t CLAP_WINDOW_MIN_MS     = 120;
constexpr uint32_t CLAP_WINDOW_MAX_MS     = 700;

// ---- Buzzer role ----
constexpr int BUZZER_PIN = 16;
