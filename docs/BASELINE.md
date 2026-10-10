# v0.3.3 baseline provenance

- Original handoff archive: `CRT_House_Console_v0_3_3.zip`
- SHA-256 of original ZIP: `87f3d0ffc5c694471a0ec654f9f08ff662978c5ce1f7c5a0c234fdebeb36ced6`
- All **22 files** inside the archive were used as the baseline. The public copy under `firmware/CRT_House_Console_v0_3_3/` differs only by replacing five private network host addresses in the sketch, replacing corresponding test-fixture addresses, and adding public-configuration comments. No behavior changes or bug fixes.
- The generated top-level README, issue records and `.gitignore` are repository documentation. The only public firmware changes are the explicitly described network redactions and comments.
- Digests of files actually published: [PUBLIC_FILE_SHA256.txt](PUBLIC_FILE_SHA256.txt)
- Board: DFR0478 FireBeetle original ESP32, GPIO25 composite
- User acceptance: Wi-Fi recovered after v0.3.3, actual HOUSE information displayed, visualizer improved; recurrence of JSON allocation error subsequently reported.
- Known errors are tracked separately under [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
- Pin configuration and network defaults are in the original `.ino`; all private LAN addresses have been replaced with RFC 5737 `192.0.2.x` placeholders; no real credentials are stored.

Recreating/importing this GitHub repository is a **version control action only**. It does not update the flashed device, Pi, HOUSE server or any other part of the installation.

**Note on provenance:** the original ZIP SHA-256 is recorded for historical identification, but the original ZIP is **not** published because it includes local LAN addresses. The repository is a sanitized public baseline, not a byte-identical copy of that ZIP.
