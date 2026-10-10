# CRT House Console

An original ESP32-based, composite-video CRT console displaying live status of the whole-house music system and other network services, with a microphone-driven Atari-style visualizer and CRT test patterns.

**Baseline:** `v0.3.3` — archived from the exact previously delivered `CRT_House_Console_v0_3_3.zip`, with only the LAN addresses replaced by documentation-only example addresses before public import. This release is a **working field baseline with outstanding bugs**, not a claim of bug-free operation.

## Field status

- The user confirmed v0.3.3 **reconnected to Wi-Fi successfully**, displays useful HOUSE status, and the visualizer improved noticeably.
- The console helped identify **MPD and Snapcast unavailable after a reboot** — a real diagnostic use.
- A later field report (2026-10-09) showed `STALE: JSON BUFFER ALLOCATION FAILED` on Now Playing, with retained track metadata but displayed `0 PRESENT / 0 AUDIBLE` counts. Both symptoms remain open; see [Known issues](docs/KNOWN_ISSUES.md).
- **No server, Pi, camera or HOUSE audio renderer modification** is required for this baseline.

## Firmware

Open [`firmware/CRT_House_Console_v0_3_3/CRT_House_Console_v0_3_3.ino`](firmware/CRT_House_Console_v0_3_3/CRT_House_Console_v0_3_3.ino) in Arduino IDE. Keep all `.cpp` and `.h` files beside the `.ino`, as in the original release archive. The tests and original README/VALIDATION notes are retained in that folder.

**Dependencies pinned for this archived source:**

- Original ESP32 / **DFRobot DFR0478 FireBeetle**, not ESP32-S3
- **esp32 by Espressif Systems 2.0.14** (not 3.x)
- `ESP_8_BIT_composite` (ZIP-installed library)
- Adafruit GFX
- **ArduinoJson 6.21.5**

**Required local configuration:** set your Wi-Fi SSID/password **and** replace the five `192.0.2.x` example addresses in the `.ino` with your own local host addresses before uploading. **Never commit your configured file**. The example addresses are RFC 5737 TEST-NET-1 documentation addresses and will not reach real devices.

## Hardware currently used

| Function | FireBeetle GPIO | Connection |
| --- | --- | --- |
| NTSC composite video | **GPIO25 / D2** | TV composite input, or RF modulator video input |
| MSGEQ7 STROBE | **GPIO26 / D3** | MSGEQ7 analyzer board STROBE |
| MSGEQ7 RESET | **GPIO27 / D4** | MSGEQ7 analyzer board RESET |
| MSGEQ7 OUT | **GPIO34 / A2** | MSGEQ7 analog OUT |
| Page button | **GPIO13 / D7** | Normally-open momentary switch to GND, internal pull-up |

Mic input uses a MAX4466 amplifier into a single mono MSGEQ7 board; the original ESP32 DAC/video output remains on GPIO25. Grounds are shared as in the working prototype.

## User interface

1. **Now Playing** — artist/title, elapsed/duration progress, queue, HOUSE volume and live presence summary.
2. **Track Queue** — selected track and next four in MPD queue order, without downloading the full queue. MPD Random may make the actual next track unpredictable.
3. **House Nodes** — currently present Snapcast/registered controller devices; offline registration history not shown.
4. **House Health** — live HOUSE/MPD/Snapserver/session status (the deleted `/diagnostics` endpoint is not used).
5. **House Systems** — LAN TCP reachability, not full service-health validation.
6. **House Clock** — Eastern time with daylight saving.
7. **Video Music** — local microphone/MSGEQ7-based CRT visualizer.
8. **CRT Test** — geometry and calibration pattern.

**Navigation:** manual on boot; tap button for next page; hold for six seconds to enable six-second auto-cycle; press during auto to stop on the current page.

## Code provenance and verification

The v0.3.3 firmware and tests were imported from the delivered v0.3.3 ZIP, with **only private LAN address literals replaced by documentation addresses**, plus comments explaining the sanitized configuration. No functional bugfix was incorporated. Test fixture addresses were likewise sanitized. See [`docs/BASELINE.md`](docs/BASELINE.md) and [`docs/PUBLIC_FILE_SHA256.txt`](docs/PUBLIC_FILE_SHA256.txt). The original release reports host-side tests with C++ stubs, *not* an ESP32 target compile. User field observations are documented separately from source-based hypotheses.

**Next development priority:** resolve the JSON buffer allocation/reconnection failure without reducing feature scope, and prevent stale metrics from looking live. Issue-ready specifications are stored in [`docs/issues/`](docs/issues/).

## Public repository privacy

This repository intentionally contains **no production SSID/passwords, host IPs, tokens, MAC addresses, or personal user paths**. `192.0.2.x` entries are non-routable examples. If you publish changes, restore example addresses before committing and use local copies for your actual environment. See [security guidance](docs/SECURITY.md).
