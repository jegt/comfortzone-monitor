# Comfortzone serial protocol

This firmware uses the proprietary Comfortzone packet format over RS485 at **19200 baud, 8 data bits, no parity and 1 stop bit**. The existing display supplies the read polling. The monitor listens to those exchanges and sends a command only after an explicit API request.

The implemented measurement and control profile is called **v1.1**. This profile label is not necessarily the complete firmware version displayed by the heat pump. Related profiles use the same framing with different field offsets or write registers; see [compatibility](compatibility.md).

## Packet layout

Offsets are zero-based. For block read/write packets:

| Byte offset | Size | Meaning |
|---|---:|---|
| 0–3 | 4 | Destination address |
| 4 | 1 | CRC-8/MAXIM of destination bytes |
| 5 | 1 | CRC-8/MAXIM of the bitwise complement of destination bytes |
| 6–9 | 4 | Source address |
| 10 | 1 | Total packet length, including the final checksum |
| 11 | 1 | ASCII command |
| 12–16 | 5 | Register prefix `01 02 03 04 0B` |
| 17–20 | 4 | Block/register identifier, most significant byte first |
| 21 onwards | variable | Payload |
| Last byte | 1 | CRC-8/MAXIM of all preceding bytes |

A block read request is 23 bytes with a one-byte requested reply size. A write request is 23 or 24 bytes, depending on whether its value uses one or two bytes. A write reply in the supported profile is 23 bytes with a one-byte reply code. Read replies can contain an entire block rather than one value.

| Command | Meaning |
|---|---|
| `R` / `r` | Block read request / reply |
| `W` / `w` | Write request / reply |
| `I` / `i` | Identity request / reply |
| `D` / `d` | Text request / reply |

Identity and text packets use different payload layouts. The measurement decoder consumes block read replies only. Capture retains the raw bytes even when a packet is not decoded as a measurement.

Replies exchange source/destination roles. Addresses must be learned from actual traffic; fixed addresses in protocol examples and test vectors must not be transmitted blindly. A valid final checksum alone is insufficient to accept an acknowledgement: the command engine also checks addressing and register bytes.

## Checksum

CRC-8/MAXIM: initial value 0, reflected polynomial `0x8c`, no final XOR. The check vector `123456789` produces `0xa1`.

```python
def crc8_maxim(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8c if crc & 1 else 0)
    return crc
```

Verify both the declared total length and final checksum before decoding a frame. The two address checksums above use the same algorithm.

## Measurement encoding

Field offsets are relative to the payload at byte 21. Two-byte values are little-endian. Signed values use signed 16-bit interpretation; the field's divisor converts the integer to its reported unit. For example, `AF 00` interpreted as a signed integer with divisor 10 gives 17.5.

| Value | Block | Payload offset | Encoding |
|---|---|---:|---|
| Outdoor temperature, TE0 | `02010000` | 46 | signed int16 / 10 |
| Flow temperature, TE1 | `02010000` | 48 | signed int16 / 10 |
| Return temperature, TE2 | `02010000` | 50 | signed int16 / 10 |
| Indoor temperature, TE3 | `02010000` | 52 | signed int16 / 10 |
| Hot gas temperature, TE4 | `02010000` | 54 | signed int16 / 10 |
| Evaporator out, TE5 | `02010000` | 56 | signed int16 / 10 |
| Evaporator in, TE6 | `02010000` | 58 | signed int16 / 10 |
| Exhaust air, TE7 | `02010000` | 60 | signed int16 / 10 |
| Hot water, TE24 | `02010000` | 94 | signed int16 / 10 |
| Compressor frequency | `0201c802` | 10 | signed int16 / 10 |
| Reported compressor power | `0201c802` | 12 | signed int16 |
| Additional heat power | `0201c802` | 14 | unsigned int16 |
| Room setpoint | `02016401` | 166 | signed int16 / 10 |
| Hot-water setpoint | `0200c802` | 171 | signed int16 / 10 |

See the [complete parameter map](parameter-map-v1.1.md), its [JSON equivalent](parameter-map-v1.1.json), and [API field definitions](fields-v1.1.json) for bit masks, offsets, widths, divisors and fault checks. Numeric sentinel values `0x8000` and `0x7fff` are treated as faults for signed fields. Temperature fault bits are checked independently.

## Supported writes

| Operation | Write register | Value width | Wire value |
|---|---|---:|---|
| Fan | `0200d503` | 1 byte | 1 low, 2 normal, 3 high |
| Room setpoint | `0200a602` | 2 bytes | °C × 10 |
| Hot-water setpoint | `02007303` | 2 bytes | °C × 10 |
| Extra hot water on | `02811900` | 2 bytes | `0x0001` |
| Extra hot water off | `02411900` | 2 bytes | `0xfffe` |

The four-byte register is big-endian; a two-byte value is little-endian. Extra hot water reads back as code 0 for off or 32 for on. Write registers and read-back fields are distinct; do not use a measurement block/offset as a write address.

The monitor learns a live route from a CRC-checked `02050000` read request and its matching reply. That reply marks the end of the supported polling cycle. The route pair must occur within one second and the learned route must be less than 15 seconds old.

A queued command waits for 100–999 ms of silence after the final poll reply, checks that the UART receive buffer is empty, then enables the driver, sends one frame, waits for transmit completion and returns to receive mode. A command times out as `bus_busy` if it cannot be sent within ten seconds. There is one transmission attempt, with no automatic retry.

Confirmation requires both an addressed, register-matched write reply and a fresh read reply matching the requested setting. Missing confirmation becomes `unconfirmed` after 15 seconds from transmission. The observed reply code is retained as data; zero is not a universal success code. Firmware updates interrupt outstanding verification and wait for an in-flight frame to finish.

See [control API](api.md#controls) for input ranges, request headers and command states. The public API deliberately exposes these four operations rather than an arbitrary register writer.
