# CRT House Console v0.3.4 — Radio display (candidate)

The field-proven v0.3.3 baseline remains unchanged on `main`. This branch
contains a **candidate CRT firmware update** for Internet radio on HOUSE
server v0.10.0. No Pi, Android, Snapcast node or camera changes.

- Consumes the existing `GET /state` source.type/status/station and MPD song
  metadata. Older HOUSE releases fall back to detecting HTTP(S) stream paths.
- For radio, **Now Playing** shows station, cleaned on-air artist/title and
  live/connecting/paused/retrying/error state. It does not show a nonexistent
  track duration, seek bar, or one-track "playlist".
- **Page 2** becomes Radio Info, with stream host, bitrate, audio format,
  and retry/error information; ordinary library Track Queue is unchanged.
- Parses common stream metadata containing `text="..."`, `song_spot=`, and
  `Media:` without applying this cleanup to regular library songs.
- A changed radio title or station name can trigger the existing overlay even
  while the MPD queue ID remains constant.
- Retains the 12,288-byte filtered JSON document after successful allocation,
  even when Wi-Fi disconnects. This mitigates one suspected recurrence path
  but is **not yet field validated as a complete memory fix**.

All source/testing uses placeholder host addresses and credentials. See the
sketch's configuration block to supply private local values.

## Validation

`bash firmware/CRT_House_Console_v0_3_4/extras/tests/run.sh` passes existing
logic, node, interface-model and view bounds tests plus new radio metadata
and source/transition tests. The simulated UI tests cannot prove physical
ESP32 compile, runtime network integration or long-term heap stability.

Original v0.3.3 running firmware remains the rollback path until accepted.
