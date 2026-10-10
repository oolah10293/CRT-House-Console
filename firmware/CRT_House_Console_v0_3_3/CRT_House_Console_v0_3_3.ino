#include "ConsoleApp.h"

// CRT HOUSE CONSOLE v0.3.3 -- DFR0478 (original ESP32), NTSC composite GPIO25.
// Keep ESP32 core 2.0.14, ESP_8_BIT_composite, Adafruit GFX, ArduinoJson 6.21.5.
// This is a sketch, not a library. Keep ALL accompanying .h/.cpp files here.
// PUBLIC COPY: local LAN addresses are example-only; configure before flashing.
// Keep credentials and network-specific addresses out of GitHub.
const char* WIFI_SSID = "PUT_YOUR_WIFI_NAME_HERE";
const char* WIFI_PASS = "PUT_YOUR_WIFI_PASSWORD_HERE";

// Same button: normally-open switch between D7 / GPIO13 and GND.
// Tap/release: next screen. Hold SIX seconds: AUTO, six seconds per screen.
// Press during AUTO: stop on the current screen; that entire press is consumed.
// Always boots in MANUAL. No special quiet-at-startup or button ritual.
const uint8_t PAGE_BUTTON_GPIO = 13;

// v0.2 was 20/20/20/20. Shift left FOUR pixels, retain horizontal width,
// and gain FOUR pixels at both the top and bottom. Text is NOT rescaled.
const uint8_t CRT_LEFT = 16, CRT_RIGHT = 24;
const uint8_t CRT_TOP  = 16, CRT_BOTTOM = 16;

// Reference circle only: nominal 4:3 pixel aspect for a 256x240 canvas.
// This is a reference, not a calibrated CRT geometry measurement.
const float CRT_PIXEL_ASPECT = 1.25f;

const ConsoleConfig config = {
  WIFI_SSID, WIFI_PASS,
  "192.0.2.10",   // EXAMPLE ONLY - Pi: HOUSE, MPD, Snapserver, DNS
  "192.0.2.20",  // EXAMPLE ONLY - NVR host: Frigate / CameraView
  "192.0.2.30",  // EXAMPLE ONLY - ANNKE NVR
  "192.0.2.40",  // EXAMPLE ONLY - Garage camera
  "192.0.2.50",  // EXAMPLE ONLY - Pole Barn camera
  "EST5EDT,M3.2.0,M11.1.0", // Eastern time, including daylight-saving changes
  8787,
  CRT_LEFT, CRT_RIGHT, CRT_TOP, CRT_BOTTOM, PAGE_BUTTON_GPIO,
  1500, // Garage connection timeout PER attempt; two misses before DOWN.
  CRT_PIXEL_ASPECT
};

void setup() { consoleBegin(config); }
void loop() { consoleLoop(); }
