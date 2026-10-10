#pragma once
#include <stdint.h>
#include <math.h>

// Pure, hardware-independent state machines. Unit-tested on the host.
namespace crt {
inline uint32_t age(uint32_t now, uint32_t then) { return now - then; }
inline float clamp(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

class Navigation {
 public:
  static constexpr uint32_t debounceMs = 30;
  static constexpr uint32_t holdMs = 6000;
  static constexpr uint32_t cycleMs = 6000;
  bool automatic = false;
  uint8_t page = 0;

  void begin(bool pressed, uint32_t now) {
    raw_ = stable_ = pressed;
    armed_ = !pressed;   // A button held at power-up must be released first.
    consumed_ = false;
    rawSince_ = pressedSince_ = cycleSince_ = now;
    automatic = false;
    page = 0;
  }

  void update(bool pressed, uint32_t now, uint8_t pages) {
    if (!pages) return;
    if (pressed != raw_) { raw_ = pressed; rawSince_ = now; }
    if (stable_ != raw_ && age(now, rawSince_) >= debounceMs) {
      stable_ = raw_;
      if (stable_) {
        pressedSince_ = now;
        consumed_ = false;
        if (automatic) {
          automatic = false; // This entire press only exits AUTO; no skip.
          consumed_ = true;
          cycleSince_ = now;
        }
      } else {
        if (armed_ && !consumed_) {
          // Use the start of the stable release, not the debounce interval.
          if (age(rawSince_, pressedSince_) >= holdMs) {
            automatic = true;
            cycleSince_ = now;
          } else {
            page = (page + 1) % pages;
            cycleSince_ = now;
          }
        }
        armed_ = true;
        consumed_ = false;
      }
    }
    if (stable_ && raw_ && armed_ && !consumed_ &&
        age(now, pressedSince_) >= holdMs) {
      automatic = true;
      consumed_ = true; // Releasing a long press must not advance the page.
      cycleSince_ = now;
    }
    // A newly detected press takes precedence over a coincident auto tick.
    if (automatic && !(raw_ && !stable_) && age(now, cycleSince_) >= cycleMs) {
      page = (page + 1) % pages;
      cycleSince_ = now;
    }
  }
  uint32_t holdProgress(uint32_t now) const {
    return stable_ && raw_ && armed_ && !consumed_
      ? age(now, pressedSince_) : 0;
  }
 private:
  bool raw_ = false, stable_ = false, armed_ = true, consumed_ = false;
  uint32_t rawSince_ = 0, pressedSince_ = 0, cycleSince_ = 0;
};

class SilenceHold {
 public:
  static constexpr uint32_t delayMs = 4000;
  void begin(uint32_t now) { lastSound_ = now; }
  bool update(bool sound, uint32_t now) {
    if (sound) lastSound_ = now;
    return age(now, lastSound_) < delayMs;
  }
 private:
  uint32_t lastSound_ = 0;
};

inline float playbackPosition(float confirmed, float duration, bool playing,
                              bool fresh, uint32_t receivedMs, uint32_t now) {
  if (!isfinite(confirmed) || confirmed < 0) confirmed = 0;
  if (playing && fresh) confirmed += age(now, receivedMs) * 0.001f;
  if (isfinite(duration) && duration > 0 && confirmed > duration) confirmed = duration;
  return confirmed;
}
} // namespace crt
