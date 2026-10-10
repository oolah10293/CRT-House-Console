CRT HOUSE CONSOLE v0.3.3
======================

INSTALL
-------
1. Extract this ZIP into a NEW folder.
2. Open CRT_House_Console_v0_3_3.ino. Keep all accompanying .cpp/.h files beside it.
3. Copy your Wi-Fi name/password into the two lines at the top of the .ino.
4. Upload with the same DFR0478 board/COM-port settings that worked for v0.3.1.

Dependencies remain unchanged:
  Espressif ESP32 board core 2.0.14
  ESP_8_BIT_composite
  Adafruit GFX
  ArduinoJson 6.21.5

No server, Pi, camera, HOUSE node, or audio-node changes are required.

PAGE ORDER
----------
1. NOW PLAYING
2. TRACK QUEUE
3. HOUSE NODES
4. HOUSE HEALTH
5. HOUSE SYSTEMS
6. HOUSE CLOCK
7. VIDEO MUSIC
8. CRT TEST

BUTTON
------
Manual at boot.
Tap/release: next page.
Hold six seconds: enable six-second AUTO cycling.
Press while AUTO is active: stop on the current page.

v0.3.3 WIFI / MEMORY HOTFIX
---------------------------
The report is: v0.3.1 connected, v0.3.2 appears not to connect with checked
credentials. Source comparison found unchanged credential handling but more
memory allocated before/during Wi-Fi startup. This makes memory pressure a
candidate, not a proven cause without the device's runtime status.

Changes:
  - Removes the 10,096-byte intermediate offline/live device merge array.
    Live devices are projected directly into the existing compact snapshot.
    Exact server IDs and explicit ownership still determine identity.
    Record limits and authoritative live counts have NOT been reduced.
  - A five-song queue snapshot drops from 2,380 to 1,240 bytes by storing a
    filename fallback in the title field instead of duplicating whole paths.
    Three copies (worker, display, queue slot) save another 3,420 bytes.
    Total removed fixed buffer payload: 13,516 bytes before small diagnostics
    overhead. This is an application-storage calculation, not measured heap.
  - The 12 KB JSON workspace is not allocated until Wi-Fi has an IP address;
    it is released while disconnected to leave reconnect memory headroom.
  - Checks Wi-Fi mode/begin success instead of ignoring startup failures.
    Failed initialization is retried; no erase-all or router changes.
  - CRT error screens show the latest Wi-Fi disconnect reason number, attempt
    count, free RAM, largest free block, and last failed allocation if any.
    A disconnect reason is evidence, not proof that a password is wrong.
    After connection succeeds these diagnostic lines do not cover normal pages.
  - Wi-Fi credentials are used from the sketch; no password logging.

No changes to visualizer, safe-area layout, clock, button rules, audio wiring,
or HOUSE playback. Queue remains page 2; Nodes remains live-only.

RETAINED v0.3.2 FEATURES
------------------------
TRACK QUEUE
  New second page. It shows the current queue entry plus up to four following
  queue entries, with title and artist. The current item is highlighted.

  This page reads a small MPD playlistinfo range directly from the already
  reachable MPD service on the Pi instead of downloading the entire queue.
  That matters because the real queue can contain well over 1,000 entries.

  When MPD Random is enabled the page explicitly says:
      RANDOM ON: NEXT TRACK NOT PREDICTABLE
  The displayed following entries are queue-order entries, not a promise that
  MPD Random will select them next.

HOUSE NODES
  Offline/remembered ID history is no longer shown. That information was useful
  for debugging identity merges but not useful as an everyday CRT page.

  Only currently present devices are displayed. If more than four are present,
  the LIVE list rotates through sets of four every six seconds while remaining
  on the HOUSE NODES page.

  Removed the recurring DETAILS STALE / STALE WAITING FOR UPDATE messages from
  this operational view. Last-known renderer names/details are allowed to remain
  while the authoritative live presence counts continue updating. If Snapserver
  itself is unavailable, the page says so rather than pretending the counts are
  current.

NOW PLAYING
  The node count continues to use the controller-aware HOUSE presence result, so
  a connected muted phone counts as PRESENT but not AUDIBLE.

OTHER v0.3.1 BEHAVIOR RETAINED
------------------------------
  - CRT-safe shifted viewport.
  - Advancing Now Playing progress/time while watched.
  - HOUSE HEALTH replaces removed /diagnostics.
  - Garage gets longer/two-attempt reachability checking.
  - Correct Eastern clock with AM/PM and EDT/EST.
  - No quiet-at-startup mic calibration.
  - Four-second visualizer silence hold.
  - Manual/auto button behavior.
  - Song-change overlay is suppressed on TRACK QUEUE and CRT TEST.
  - Reusable ArduinoJson document remains 12 KB (v0.3.1 allocation hotfix).

QUEUE IMPLEMENTATION NOTE
-------------------------
MPD's playlistinfo command accepts a bounded START:END queue range. v0.3.2 asks
for at most five entries beginning at the current zero-based song position. This
keeps RAM/network use bounded instead of asking house-audio-server /queue for the
entire current queue.

VALIDATION BOUNDARY
-------------------
Host-side logic/layout/syntax tests pass, including all eight page layouts and
safe-area checks. These tests use stubs and are NOT an ESP32 target build.
Physical Wi-Fi connection, runtime memory headroom and the MPD queue page still
need the FireBeetle/Pi test. Successful host checks do not establish any of those.
See VALIDATION.txt.
