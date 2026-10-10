# Public repository security and privacy

This is a **public** electronics/firmware project. Do not commit secrets or a snapshot of your home network.

- Do not commit real Wi-Fi network names or passwords, tokens, API keys or camera/NVR credentials.
- Do not commit real LAN/WAN IPs, MAC addresses, device serial numbers, user/computer names, or local filesystem paths.
- The tracked sketch uses RFC 5737 documentation addresses (`192.0.2.0/24`), **not** working defaults. Replace them only in your local copy before uploading to a device.
- Before publishing edits to the sketch, change the IP addresses and Wi-Fi strings back to example placeholders.
- If a secret ever lands in a commit, removing it from the latest file is insufficient: rotate the secret and review repository history.

Please keep bug reports free of screenshots/logs that expose private network addresses, authentication headers, or unique device identifiers.
