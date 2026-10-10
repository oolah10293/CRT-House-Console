# Cached track metadata can appear with misleading zero node counts

**State:** OPEN  
**Priority:** P2 — UI/data validity  
**Baseline:** v0.3.3  
**Observed with:** JSON allocation failure, 2026-10-09

## Observed (confirmed)

CRT Now Playing displayed an old track and progress with `STALE: JSON BUFFER ALLOCATION FA...` while also saying `NODES 0 PRESENT / 0 AUDIBLE`. Those counts are **not reliable evidence of node absence** while the display cannot receive current state.

## Source evidence

- `ConsoleApp.cpp::drawNowPlaying()` prints numeric counts when `nodes.info.have && nodes.snapReachable`, without also requiring `nodes.info.ok` and sufficient freshness.
- The JSON allocation failure can interrupt the reader while leaving previously sampled state and displayed metadata available.

## Required fix / acceptance criteria

- Keep the original **last-known** values visible if useful, but mark stale vs current clearly.
- When no valid fresh node snapshot exists, show `NODES: UNAVAILABLE` / `NODES: STALE` (or `--`) rather than authoritative-looking `0 PRESENT / 0 AUDIBLE`.
- A genuine fresh 0/0 response may still display 0/0.
- Preserve the intended controller-aware count: a muted-but-connected HOUSE phone is **present, not audible**, and must not be counted twice with its own renderer.
- Test Wi-Fi loss, Snapserver loss, failed JSON allocation, stale values and normal recovery independently.
