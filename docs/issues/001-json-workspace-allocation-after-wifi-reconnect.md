# JSON workspace allocation failure during runtime

**State:** OPEN  
**Priority:** P1 — reliability regression  
**Baseline:** v0.3.3  
**First user-visible report:** 2026-10-09

## Observed (confirmed)

After v0.3.3 had been working in daily use, the CRT displayed:

`STALE: JSON BUFFER ALLOCATION FAILED`

Track metadata and the progress bar were still visible from the last successful update. The user also observed `NODES 0 PRESENT / 0 AUDIBLE` in the same state. Wi-Fi/server status at the precise failure moment was not independently measured.

## Source evidence (confirmed)

- `ConsoleApp.cpp::getJson()` returns `JSON BUFFER ALLOCATION FAILED` when `doc.capacity()==0`, before it sends an HTTP request.
- `ConsoleApp.cpp::networkTask()` initializes `DynamicJsonDocument doc(0)`; after Wi-Fi connects it attempts `doc=DynamicJsonDocument(12288)`. It sets `doc=DynamicJsonDocument(0)` when Wi-Fi disconnects, requiring a later reallocation.
- The v0.3.3 reduction in static buffers fixed the earlier Wi-Fi startup symptom but did not prevent the later JSON allocation failure.

## Hypothesis (not proven)

A Wi-Fi disconnect/reconnect may fragment/reduce available contiguous heap enough to prevent the subsequent 12 KiB allocation. Heap exhaustion or another allocation pattern is also possible. The observed error establishes failed buffer availability, **not the precise cause of it**.

## Required fix / acceptance criteria

- Keep the response parser operational through repeated Wi-Fi disconnect/reconnect cycles **without resizing away needed functionality**.
- Avoid repeated freeing/reallocating of the JSON buffer on every transient disconnect, unless measured memory requirements justify it.
- On allocation failures, report free heap, largest contiguous allocatable block and a useful error; continue page/button/local visualizer operation.
- Distinguish a temporarily unavailable HOUSE service from a local JSON allocation failure.
- Stress test repeated network outages, subsequent re-joins, sustained polling and page cycling on a physical DFR0478. Confirm live song updates recover without reset and never get stuck on `JSON BUFFER ALLOCATION FAILED`.
- Record physical ESP32 results separately from host stubs.

**Do not alter the Pi or HOUSE audio nodes to work around a display-side allocation bug.**
