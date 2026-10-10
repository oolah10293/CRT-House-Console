CRT HOUSE CONSOLE v0.3.4 — LIVE HOUSE RADIO DISPLAY
================================================

This is a *candidate* update of the field-working v0.3.3 firmware.
It changes only the DFR0478 CRT console. Nothing is installed on the Pi,
Snapcast clients, cameras, or other devices.

INSTALL
-------
1. Extract the ZIP to a new folder.
2. Open CRT_House_Console_v0_3_4.ino in Arduino IDE.
3. Enter your Wi-Fi SSID and password in the two placeholders at the top.
4. Use the existing board settings and click Upload.

This PUBLIC source uses non-routable RFC5737 example addresses (192.0.2.x).
Enter your actual LAN host addresses privately, alongside Wi-Fi credentials,
before flashing. Never commit the locally configured sketch.

PINOUT — unchanged
------------------
Original DFRobot FireBeetle ESP32 DFR0478:
Composite NTSC: GPIO25
MSGEQ7 RESET: GPIO27; STROBE: GPIO26; OUT: GPIO34
Page button: GPIO13 to GND (internal pull-up)

Libraries/board versions — unchanged:
  Espressif Arduino-ESP32 2.0.14
  ESP_8_BIT_composite / ESP_8_BIT_GFX
  Adafruit GFX
  ArduinoJson 6.21.5

RADIO BEHAVIOR
--------------
When the HOUSE server's /state.source.type is "radio":
  1. NOW PLAYING uses the saved station name and separate current artist/title.
     It shows live/connecting/retrying/paused/error status from source.status.
     No 0:00 duration or dead seek/progress bar; bitrate only when reported.
  2. The second page becomes RADIO INFO: station, stream host, decoded audio
     format, bitrate, and retry/error status. It does not imply a track queue.
  3. Song-change overlays react to updated on-air artist/title even though
     MPD songId remains constant for the station.
  4. Raw Icecast-like title text (text="..." song_spot="..." Media:...)
     is parsed to separate a human-readable track title and artist.

If the server predates v0.10.0 and does not expose /state.source, the CRT
recognizes a HTTP(S) stream URL as radio, with less specific status reporting.

Ordinary file/album/playlist playback keeps all original pages and behavior.
No server endpoints, station bookmarks, or HOUSE playback settings are changed.

MEMORY RELIABILITY
------------------
The 12,288-byte *filtered* JSON workspace is still only first allocated after
Wi-Fi connects. Once acquired, it now remains allocated across Wi-Fi drops,
avoiding repeated requests for a large contiguous heap block at each reconnect.
This addresses a suspected failure mechanism from the v0.3.3 incident, but
unattended device operation still needs verification. It is NOT presented as
field-proven or as a complete fix for all heap fragmentation.
Only selected source/MPD fields are copied. No giant station list is fetched.

WHAT WAS TESTED
---------------
The included host test script compiles and runs the logic, device-model,
radio-metadata, and full display/interface-model tests. It checks live radio,
title extraction, older-server fallback, queue bypass, source switching,
radio pause status, overlay update, geometry clipping, and existing behavior.
Tests use host stubs. A physical Arduino-ESP32 build was not available in this
environment; a successful upload and on-device field test remain necessary.

When reporting a problem, give the screen text/behavior. No Serial Monitor
or PowerShell logging is necessary to run or check this version.
