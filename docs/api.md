# Local HTTP API

HTTP endpoints are unauthenticated. Arduino OTA uses its separate password. No CORS permission is granted; the custom control header is a request check rather than authentication. Use a trusted local network.

## Dashboard and status

`GET /` serves the live dashboard and control forms. It receives status, measurements and raw chunks through SSE. Header diagnostics show RSSI/channel, AP BSSID, IP, uptime, firmware version, free heap and CRC/UART errors. The dashboard marks measurements and status stale after their respective age thresholds.

`GET /api/status` returns firmware and boot IDs, current IP/uptime, Wi-Fi connection/attempt counts, retry interval, AP BSSID/channel/RSSI, heap/PSRAM size, receive bytes, parser frames/bad-CRC count, UART errors, next capture sequence, ring capacity, serial settings, transmit counters and decoder profile. Live network identifiers are included in the local diagnostics response.

## Values

`GET /api/values` returns `boot_id` and a `measurements` array. Each entry has:

| Key | Meaning |
|---|---|
| `id` | Stable English field identifier |
| `label`, `group` | Display label and grouping |
| `loggamera_name` | Exact Swedish compatibility name, or null |
| `value`, `unit`, `kind` | Reading, unit and rendering category |
| `source`, `evidence` | Mapping/derivation and validation category |
| `state` | `waiting`, `ok`, `stale` or `sensor_fault` |
| `age_ms` | Age of received reading, or null |

Missing and fault values are null. Stale values retain their age and value. See [measurements](measurements.md) for all fields, special encodings, derived values and semantics.

## Event stream

`GET /api/events` returns `text/event-stream`. Up to three simultaneous clients are supported; additional connections receive HTTP 503.

| Event | JSON data |
|---|---|
| `status` | Full status on connection and every five seconds |
| `measurements` | Full values on connection and with periodic status |
| `traffic` | `boot_id`, `sequence`, `ms`, raw chunk `hex` |
| `gap` | `boot_id`, `dropped_chunks` |

Traffic cursor IDs are `<boot_id>:<sequence>`. Supply `Last-Event-ID` on reconnection; browser clients can also use the `cursor` query parameter. A different boot ID resets the cursor to the current buffer. Overwritten data produces a gap event. Chunks are not frame boundaries.

A partially transmitted event is replayed from the last completely received cursor after reconnection. Stalled subscribers are disconnected after ten seconds without send progress. The dashboard retries terminal errors after two seconds; normal EventSource reconnects use Last-Event-ID. A restart or OTA update clears or interrupts the RAM capture.

## Polling capture

`GET /api/traffic?after=N` returns `boot_id`, `oldest_sequence`, `next_sequence`, `dropped_chunks`, and at most 64 chunks. Each chunk contains sequence, monotonic milliseconds and hex bytes. Advance after the last fully received sequence. Check boot IDs and gap counts before reconstructing frames. The collector can use this endpoint with `--poll`; SSE is the default.

## Controls

| POST endpoint | `value` |
|---|---|
| `/api/control/fan` | 1 low, 2 normal, 3 high |
| `/api/control/room_temperature` | 10.0–50.0 °C, one decimal place |
| `/api/control/hot_water_temperature` | 10.0–60.0 °C, one decimal place |
| `/api/control/extra_hot_water` | 0 off, 1 on |

Requests require `Content-Type: application/x-www-form-urlencoded`, `X-Comfortzone-Control: 1` and exactly one field named `value`. The legacy `X-EX50-Control: 1` is accepted as an alias. Values use strict decimal syntax: whitespace, exponent notation, nonfinite values, extra precision and out-of-range values are rejected. GET endpoints never issue commands.

Example that changes the room target to 21 °C:

```sh
curl -X POST http://comfortzone-monitor.local/api/control/room_temperature \
  -H 'Content-Type: application/x-www-form-urlencoded' \
  -H 'X-Comfortzone-Control: 1' --data 'value=21.0'
curl http://comfortzone-monitor.local/api/control
```

A valid request returns HTTP 202 with the latest command state. Retain its `boot_id` and command `id`, then poll `GET /api/control`. The response includes command, requested value, acknowledgement/read-back flags, raw reply code, transmission timestamp and transmitted hex.

| State | Meaning |
|---|---|
| `idle` | No command has been queued since this boot |
| `queued` | Waiting for an eligible bus gap |
| `verifying` | Sent once, awaiting acknowledgement and fresh read-back |
| `confirmed` | Both confirmation conditions satisfied |
| `bus_busy` | No transmission opportunity within ten seconds |
| `unconfirmed` | No complete confirmation within 15 seconds of sending |
| `interrupted` | Outstanding command interrupted, for example by OTA |

HTTP 202 is acceptance rather than confirmation. If another client supersedes the stored result, its command ID must not be mistaken for yours. Only the latest result is retained in RAM, and reboot clears it. An unconfirmed command may have taken effect; inspect the current setting before manually retrying. Reply codes are raw protocol data rather than universal success/error numbers.

Errors: 400 for invalid request/header/value, 409 for another active command, and 503 for stale/unavailable current setting or unavailable bus/update in progress. The command engine sends once, never automatically retries, and requires fresh route learning plus acknowledgement/read-back. See [wire protocol and bus timing](protocol.md#supported-writes).
