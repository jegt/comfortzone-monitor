# Compatibility and validation

The framing comes from the Comfortzone protocol family supported by the pinned [qix67 library](../libraries/comfortzone_heatpump/README.txt). The monitor uses its parser with a separate v1.1 measurement and control profile.

## Supported profile

The implemented profile has been validated on one Comfortzone EX50. Its field offsets and write registers differ from upstream profiles named 1.60, 1.80 and 2.21. A shared frame format does not guarantee a shared block layout. Compatibility with other models or firmware variants requires comparing block identifiers, field meanings, scaling and addressing against the [protocol reference](protocol.md).

All 70 direct fields have been observed in incoming traffic. The API adds five derived fields, for a total of 75. Thirty-six selected readings/settings matched an independent time-series source at near-time samples. That is validation of those samples, not every possible operating state. The four commands were validated by reapplying existing settings and checking acknowledgements plus subsequent read-back.

## Interpretation limits

- Compressor frequency and power were zero in the comparison samples. Nonzero scaling still needs independent operating-cycle validation.
- Reported compressor power is not asserted to be measured electrical input power. The API does not provide a COP calculation.
- Alarm flags and acknowledgement bits are separate raw protocol bits. Their combined alarm lifecycle has not been validated by inducing faults.
- Hot-water priority remains a raw code; there is no confirmed Low/Normal/High interpretation for this profile.
- Extra-hot-water on is covered by protocol-vector tests but was not activated in live acceptance. Off was confirmed live.
- Energy/runtime counters, input power, pressures, expansion-valve values and additional version-dependent sensors remain unexposed because their layout is not established for this profile.
- Derived power splits use valve direction and reported power. They are not independent heat meters. Adjacent polling blocks can briefly represent different instants during transitions.

## Tests and runtime checks

`sh tools/test_all.sh` runs five Python tooling tests and the C++ measurement/control tests with AddressSanitizer and UndefinedBehaviorSanitizer. Synthetic fixtures cover all 75 fields, bit patterns, signed values, faults, truncation/CRC rejection, derived-field freshness, input validation, route pairing, quiet-gap gating, acknowledgement/address checks, fresh read-back, timeouts, interrupted updates and timer rollover.

Firmware 0.8.3 was built and uploaded through Arduino OTA. After reboot, 75 measurements resumed, Swedish name metadata, the dashboard and an SSE measurement event were verified, with zero UART/CRC errors in the acceptance sample. Desktop/mobile rendering and stale/reconnection behavior were also checked during earlier acceptance. These results do not replace validation on a different installation.

## Testing another heat pump

Begin with passive reception and the display connected. Check CRC/UART diagnostics and compare temperatures, settings and units with independently visible values. Confirm the required end-of-cycle request/reply pair and profile-specific write registers before enabling controls. Use the collector's bounded capture and freshness metadata to identify gaps or missing blocks.
