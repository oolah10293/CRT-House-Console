#pragma once
#include <stdint.h>

struct ConsoleConfig {
  const char* ssid;
  const char* password;
  const char* pi;
  const char* lenovo;
  const char* annke;
  const char* garage;
  const char* pole;
  const char* timezone;
  uint16_t housePort;
  uint8_t left, right, top, bottom;
  uint8_t buttonPin;
  uint16_t garageTimeoutMs;
  float pixelAspect;
};
void consoleBegin(const ConsoleConfig& config);
void consoleLoop();
