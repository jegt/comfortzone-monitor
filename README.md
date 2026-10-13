# Comfortzone local monitor

Local Wi-Fi monitoring and control for Comfortzone heat pumps, using a Waveshare ESP32-S3-RS485-CAN board. A browser dashboard, JSON API and event stream expose measurements directly from the heat pump's RS485 bus. Measurements are available directly on the local network.

Firmware 0.8.3 exposes 75 fields and four controls: fan mode, room setpoint, hot-water setpoint and extra hot water. Monitoring listens to the existing control panel's polling; transmission happens only after an explicit control request.

This implementation was validated on one EX50 using the **v1.1 protocol profile**. Other firmware variants may use different offsets or command registers. Validation covers received fields and reapplication of existing settings; nonzero power scaling and alarm lifecycle interpretation remain incomplete. See [validation and limitations](docs/compatibility.md).

## Hardware

- Waveshare ESP32-S3-RS485-CAN or external-antenna `-U` variant, 16 MB flash and 8 MB OPI PSRAM.
- A six-contact modular cable/breakout for the heat pump's RS485 connector. The upstream README calls this RJ11 6P6C; such cables are commonly sold as RJ12.
- A compatible power source and USB cable for the first upload.

The board uses TX GPIO17, RX GPIO18 and direction-enable GPIO21. The bus runs at **19200 baud, 8N1**, with the original heat-pump display still connected.

The upstream project gives this six-contact connector pinout:

| Pin | Connection |
|---|---|
| 1 | 24 V (25 V without load) |
| 2 | GND |
| 3 | RS485 B |
| 4 | RS485 A |
| 5 | 24 V (25 V without load) |
| 6 | GND |

Source: [qix67's README](libraries/comfortzone_heatpump/README.txt). The 25 V reading is its reported no-load measurement, rather than a guaranteed voltage specification. Pin numbers depend on viewing orientation; confirm the actual contacts and cable mapping.

[Hardware and wiring notes](docs/hardware.md) cover power, termination and USB adapters. Confirm wiring for your installation before applying power.

## Setup

Install [Arduino CLI](https://arduino.github.io/arduino-cli/) and Python 3. The tested toolchain is Arduino CLI 1.5.1, Espressif Arduino core 3.3.11 and FastCRC 1.44.0. From this repository's root:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install 'FastCRC@1.44.0'
cp comfortzone_wifi/secrets.example.h comfortzone_wifi/secrets.h
chmod 600 comfortzone_wifi/secrets.h
```

Edit `comfortzone_wifi/secrets.h` with your Wi-Fi name/password, hostname and a long random OTA password. You can generate a password with `python3 -c 'import secrets; print(secrets.token_urlsafe(32))'`. The configuration and build outputs are ignored by Git; compiled firmware contains the credentials.

```sh
python3 tools/device.py build
arduino-cli board list
# Use the USB port reported above:
python3 tools/device.py usb --port /dev/cu.usbmodem101
```

On Linux the USB port will usually look like `/dev/ttyACM0`; on Windows use the reported COM port. The helper selects ESP32-S3, 16 MB flash, OPI PSRAM and two 3 MB application slots for OTA.

After it connects, open [http://comfortzone-monitor.local/](http://comfortzone-monitor.local/) or the address reported by your DHCP server. If you changed the hostname, use that instead. Subsequent uploads use password-protected Arduino OTA:

```sh
python3 tools/device.py status --host comfortzone-monitor.local
python3 tools/device.py ota --host comfortzone-monitor.local
```

If OTA discovery fails for `.local`, pass the IP reported by `/api/status`. OTA requires a connection back from the board to the upload computer. Keep the USB recovery path accessible. Changing the OTA password requires uploading with the currently running password first; the helper assumes old and new passwords match.

HTTP monitoring and controls have **no authentication**. The OTA password protects firmware uploads only. Run this on a trusted local network; the write header is a request check, not an access credential.

## Local API

| Endpoint | Purpose |
|---|---|
| `GET /` | Live dashboard and control forms |
| `GET /api/status` | Firmware/boot ID, uptime, Wi-Fi and UART/parser diagnostics |
| `GET /api/values` | Measurements, units, provenance and freshness |
| `GET /api/events` | Server-sent events: status, measurements, raw traffic and gaps |
| `GET /api/traffic?after=0` | Bounded raw-byte capture with a sequence cursor |
| `GET /api/control` | Latest command result and confirmation state |
| `POST /api/control/fan` | Fan: 1 low, 2 normal, 3 high |
| `POST /api/control/room_temperature` | Room setpoint: 10.0–50.0 °C |
| `POST /api/control/hot_water_temperature` | Water setpoint: 10.0–60.0 °C |
| `POST /api/control/extra_hot_water` | Extra hot water: 0 off, 1 on |

Measurement objects include stable English `id`, English `label`, exact Swedish `loggamera_name` where confirmed, `value`, `unit`, `group`, `kind`, `source`, `evidence`, `state` and `age_ms`. States distinguish missing, sensor fault, stale and current readings. Missing/faulty values are null; stale values retain their age. There are 70 directly mapped fields and five derived fields, including settings and protocol flags as well as sensors.

`loggamera_name` matches 36 existing Swedish Influx `name` tags; the remaining 39 fields return null. Matching names does not convert values: `extra_hot_water_code` is 0/32 on the wire, while the historical Influx series uses 0/1. Clock year is represented as a full year.

Writes require `Content-Type: application/x-www-form-urlencoded`, `X-Comfortzone-Control: 1` and one form field `value`. The previous `X-EX50-Control: 1` header is also accepted for compatibility. Temperature inputs permit one decimal place. A 202 response means queued; successful confirmation requires both an addressed acknowledgement and a fresh matching read-back. The engine sends once, waits for an appropriate bus gap, and never automatically retries an ambiguous command. See the [full API guide](docs/api.md) and [wire protocol reference](docs/protocol.md).

## Capture and reliability

```sh
mkdir -p private
python3 tools/device.py collect --host comfortzone-monitor.local --seconds 120 --output private/capture.jsonl
```

The UART owner runs independently of web requests. Capture uses a bounded RAM ring: 2,048 chunks with PSRAM, 512 otherwise. Records are byte chunks, not complete packets. Overwritten data is reported as gaps; reboot IDs distinguish fresh buffers. The collector resumes SSE using its cursor and can fall back to polling with `--poll`. Capture files can contain device identifiers and household history; keep them private.

Wi-Fi retries indefinitely, with a new explicit attempt every 30 seconds offline. Explicit attempts scan all channels and prefer the strongest eligible AP. This does not implement proactive roaming while already connected. Capture continues within its RAM limits while Wi-Fi is unavailable. SSE supports three concurrent clients; extra clients receive 503 and may retry.

## Tests

Offline tests need Python 3 and a C++17 compiler:

```sh
sh tools/test_all.sh
```

The included measurement and bus-gap fixtures are **synthetic**, with invented addresses and values. They exercise the decoder, all bit patterns, fault handling, derived values, route learning, bus gating, control validation, acknowledgement/read-back, timeout and rollover behavior. See [compatibility and validation](docs/compatibility.md) for the acceptance results and remaining interpretation limits.

An optional live SSE check is available with `python3 tools/verify_sse.py --host comfortzone-monitor.local`. Close other SSE subscribers first; it uses all three slots and verifies fourth-client rejection. It sends no heat-pump commands.

## Documentation

[Documentation index](docs/README.md) links to hardware and wiring, serial framing and checksums, complete field/register maps, the HTTP API, control timing, capture and Wi-Fi behavior, firmware updates, and compatibility limits.

Local configuration and captures are ignored by Git. Firmware binaries built with your configuration contain its credentials; keep those artifacts private.

## License and acknowledgements

GPL-3.0-or-later; see [LICENSE](LICENSE). The vendored parser is based on [qix67/comfortzone_heatpump](https://github.com/qix67/comfortzone_heatpump), commit `c0a896db8163d721e373c983bc651e7863e8c28b`, with its original license and attribution preserved. FastCRC is installed separately through Arduino Library Manager.

Hardware sources: [Waveshare product/wiki](https://www.waveshare.com/wiki/ESP32-S3-RS485-CAN) and [schematic](https://files.waveshare.com/wiki/ESP32-S3-RS485-CAN/ESP32-S3-RS485-CAN-Schematic.pdf). This is an independent community project.
