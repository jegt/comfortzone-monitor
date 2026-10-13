# Runtime, capture and firmware updates

## Serial ownership and buffering

A dedicated FreeRTOS task owns RS485, independently of web requests. Its UART receive buffer is 8 KB. Raw bytes are grouped into chunks of up to 64 bytes, each with a sequence number and monotonic timestamp. Chunks are not packet boundaries; reconstruct packets using declared length and checksum.

The capture ring holds 2,048 chunks in PSRAM, falling back to 512 chunks if PSRAM allocation fails. Old chunks are overwritten; the API reports the gap. There is no continuous flash logging or unbounded device filesystem growth. Boot IDs distinguish ring resets. UART error counters flag receive problems; OTA deliberately pauses capture.

The upstream parser adapter remains receive-only. The separate control engine and UART owner manage the only transmit path, coordinating it with capture and firmware updates.

## Collector

```sh
mkdir -p private
python3 tools/device.py collect --host comfortzone-monitor.local \
  --seconds 120 --output private/capture.jsonl
```

The collector runs only while this command is active. Without `--seconds`, it runs until interrupted. It writes complete SSE events as JSONL, records connection failures, reconnects, resumes from its last complete cursor and retains gap/reboot information. `--poll` uses the polling capture endpoint. Device captures can include identifiers and household history; keep them private.

## Wi-Fi recovery

The board connects immediately at boot and retries indefinitely while the configured network is unavailable. Framework automatic reconnection is supplemented by a fresh explicit attempt every 30 seconds offline. After a connected-to-disconnected transition, automatic reconnection gets a full interval before that attempt.

Explicit attempts scan all channels and prefer the strongest eligible AP with the configured SSID. No BSSID is locked. This chooses an AP when connecting; it does not proactively roam while connected. Status distinguishes explicit attempts from observed successful connections. Timers tolerate rollover.

Serial capture continues while Wi-Fi is offline, within the RAM ring's limits. Existing subscribers are closed on disconnect and reconnect when the network returns. No blocking wait-for-Wi-Fi loop prevents capture.

## Firmware updates

The first upload uses USB. Later uploads use password-protected Arduino OTA on UDP port 3232. Keep the 16 MB flash layout with two 3 MB application slots and retain USB access for recovery. During an update the firmware waits for an in-flight transmit frame to finish, returns the driver to receive mode and interrupts outstanding command verification.

Arduino OTA first sends a UDP invitation and authenticates. The board then opens a TCP connection back to the upload computer to download the image. The return connection must be reachable through the local network/firewall. A VPN/subnet route that permits access to HTTP does not necessarily permit this callback; upload from a host reachable on the board's network if needed.

If Arduino CLI cannot discover the network port, try the device's current IP instead of `.local`. A direct invocation of the ESP32 core's bundled `espota.py` can bypass discovery. It still needs authentication and the return TCP connection. Password rotation requires authenticating with the currently installed OTA password; the helper normally assumes old and new passwords are identical.

After uploading, verify the expected firmware ID, changed boot ID, low uptime, resumed incoming frames, fresh measurements and UART/CRC diagnostics. A successful upload alone is not runtime verification.
