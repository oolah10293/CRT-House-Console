# Known issues — v0.3.3 baseline

This lists **issues still open** based on actual user observations and inspection of the archived source. Do not mix guesses about root cause with observations.

| ID | Issue | Status | Detail |
| --- | --- | --- | --- |
| 001 | [Runtime JSON workspace allocation failure](issues/001-json-workspace-allocation-after-wifi-reconnect.md) | **OPEN** | Reproduced on the user's CRT after successful v0.3.3 operation; may be triggered by Wi-Fi reconnect, but trigger is not confirmed. |
| 002 | [Misleading node counts while stale](issues/002-stale-node-counts-displayed-as-live.md) | **OPEN** | When state is stale/JSON unavailable, a cached song and `0 PRESENT / 0 AUDIBLE` can be displayed together as though both were current. |

## Resolved/historical issues (not active bugs)

- **v0.3.2 Wi-Fi join regression:** user reported failure to join; v0.3.3 memory/Wi-Fi changes restored connectivity in the user's field test. The precise cause was not proven by instrumentation.
- **v0.3.1 large JSON allocation failure:** reduced from 24 KiB to 12 KiB, restoring operation at the time. A separate/recurrent 12 KiB allocation failure was later observed in v0.3.3 (Issue 001); it is **not resolved**.
- **Old `/diagnostics` 404:** endpoint was intentionally removed on the HOUSE server. The CRT page was replaced with House Health using retained endpoints.
- **Offline Snapcast identity clutter:** removed from House Nodes in v0.3.2, leaving live-only entries.
- **Visualizer overly driven by volume/noise:** improved in v0.3.3 according to user feedback; do not silently rewrite the algorithm without new evidence.

## Validation boundaries

The v0.3.3 archive's `VALIDATION.txt` records host-side C++ tests but explicitly says an ESP32 target build and field memory measurements were not performed in that delivery. The user did subsequently upload and run v0.3.3 successfully, but that does not establish exhaustive reliability or an independent target build in this repository.
