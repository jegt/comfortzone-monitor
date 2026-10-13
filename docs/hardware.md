# Hardware and wiring

The supported board is Waveshare ESP32-S3-RS485-CAN, including the `-U` variant with an external antenna connector. It has an integrated RS485 transceiver, 16 MB flash and 8 MB OPI PSRAM. CAN is not used by this firmware.

## Heat-pump connector

The upstream EX50 documentation describes two parallel RS485 sockets: one for the original display, one for an additional device. Keep the display connected because it supplies the polling traffic the monitor consumes.

| Pin (upstream numbering) | Connection |
|---|---|
| 1 | 24 V (25 V without load) |
| 2 | GND |
| 3 | RS485 B |
| 4 | RS485 A |
| 5 | 24 V (25 V without load) |
| 6 | GND |

Source: the [upstream README](https://github.com/qix67/comfortzone_heatpump/blob/c0a896db8163d721e373c983bc651e7863e8c28b/README.txt). The 25 V figure is a reported no-load measurement, not a guaranteed voltage specification. Confirm connector viewing orientation and actual cable contact mapping. Six contacts in six positions are often sold as RJ12, although the upstream text uses RJ11 6P6C.

Connect A and B to the corresponding RS485 terminals. The supply contacts must not be connected to a USB RS485 adapter or an Ethernet socket. RS485 uses an active transceiver, not a passive USB cable converter.

## ESP32 interface and power

Waveshare schematic GPIO assignments: TX 17, RX 18 and DE 21. Monitoring keeps DE low; the UART owner briefly enables it for explicit commands and waits for transmit completion before returning to receive mode. Keep debug output off the RS485 UART.

The board accepts 7–36 V on its DC terminal. When using the heat pump's nominal 24 V supply, verify polarity, actual voltage and available current for the board's load. Waveshare specifies that USB and terminal power must not be applied simultaneously.

## Termination and adapter selection

RS485 termination belongs at the physical ends of the bus. The firmware does not establish whether an existing display/pump combination already includes termination. The Waveshare board has selectable termination; inspect its jumper and bus topology before changing it. The upstream project does not provide a definitive instruction to add or remove a 120 Ω resistor for every installation.

For computer-side capture, a USB RS485 adapter must match the required electrical interface. Adapter models differ in connector, isolation and termination. Automatic-direction adapters can transmit without exposing DE/RE pins. Bus commands still require the Comfortzone framing and coordinated timing described in [protocol](protocol.md).

## References

- [Waveshare board documentation](https://www.waveshare.com/wiki/ESP32-S3-RS485-CAN)
- [Waveshare schematic](https://files.waveshare.com/wiki/ESP32-S3-RS485-CAN/ESP32-S3-RS485-CAN-Schematic.pdf)
- [Upstream project](https://github.com/qix67/comfortzone_heatpump)
- [Upstream hardware discussion](https://github.com/qix67/comfortzone_heatpump/issues/2)
