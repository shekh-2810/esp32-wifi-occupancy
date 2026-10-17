#pragma once
// Copy this file to secrets.h (which is gitignored) and fill in your values.

// Wi-Fi network shared by all nodes and the server.
#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Salt used when hashing MAC addresses. MUST be identical on every node so the
// same physical device gets the same ID everywhere (needed for fusion).
// Pick your own random value and keep it private.
#define HASH_SALT 0xC001D00Du
