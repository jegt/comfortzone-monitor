# Measurements and field definitions

`GET /api/values` and SSE `measurements` expose the same 75 fields: 70 direct protocol values and five derived values. The table below lists every API identifier, unit and block offset. [fields-v1.1.json](fields-v1.1.json) also includes signedness, widths, masks, inversion, additive transforms and fault checks.

## Special encodings

- Temperature and setpoint values are signed little-endian integers divided by 10. Temperature sensor fault bits and reserved signed values are handled separately.
- Fan mode is 1 low, 2 normal, 3 high.
- Extra hot water reads as code 0 off or 32 on. The control API accepts 0/1. An importer matching the historical boolean series must normalize the reading from 0/32 to 0/1.
- Year adds 2000 to the one-byte value; it is exposed as a full year.
- Heating and hot-water valve values use bit `0x04` at payload offset 26 in `02010000`, with heating inverted.
- Alarm and acknowledgement fields expose individual masks as values 0/1. They do not define a combined active-alarm state.
- Hot-water priority and other protocol codes remain numeric where their full meaning is unconfirmed.

## Derived values

`compressor_running` is frequency > 0. The four compressor/additional power splits use the hot-water valve value: an active hot-water direction allocates the corresponding reported power to hot water; otherwise it allocates it to heating. Missing or faulty dependencies propagate, and age follows the older dependency. These are local calculations, not additional wire fields.

## Freshness and metadata

Measurements start as `waiting`, with null value and age. Faulty readings use `sensor_fault` and null value. Received values older than 30 seconds are `stale` and retain their value and age; newer valid values are `ok`. Units are defined per field rather than inferred from identifiers.

The `evidence` field is `influx`, `mapped` or `derived`. `influx` denotes an independent sample comparison; it is not a guarantee for every value. `mapped` denotes a known wire position with incomplete live interpretation, and `derived` denotes a local calculation. The `source` label identifies the mapping family or derivation.

The `loggamera_name` field preserves exact Swedish names for 36 matching historical series. Unmatched fields return null. It is compatibility metadata; the monitor's English `id` is the stable identifier. Names do not imply identical value representations, particularly for extra hot water or year.

## Complete API field list

| ID | Meaning | Unit | Block | Payload offset |
|---|---|---|---|---:|
| `outdoor` | Outdoor | °C | `02010000` | 46 |
| `indoor` | Indoor | °C | `02010000` | 52 |
| `hot_water` | Hot water | °C | `02010000` | 94 |
| `flow` | Flow | °C | `02010000` | 48 |
| `return` | Return | °C | `02010000` | 50 |
| `hot_gas` | Hot gas | °C | `02010000` | 54 |
| `evaporator_out` | Evaporator out | °C | `02010000` | 56 |
| `evaporator_in` | Evaporator in | °C | `02010000` | 58 |
| `exhaust_air` | Exhaust air | °C | `02010000` | 60 |
| `compressor_frequency` | Compressor frequency | Hz | `0201c802` | 10 |
| `compressor_power` | Compressor power (reported) | W | `0201c802` | 12 |
| `additional_power` | Additional heat | W | `0201c802` | 14 |
| `room_setpoint` | Room setpoint | °C | `02016401` | 166 |
| `hot_water_setpoint` | Hot-water setpoint | °C | `0200c802` | 171 |
| `heating_curve` | Heating curve | — | `0201c802` | 108 |
| `highgaspresalarm` | High gas pressure flag | — | `02010000` | 29 |
| `hot_water_overheatalarm` | Hot-water overheat flag | — | `02010000` | 30 |
| `lackofcoolantalarm` | Low refrigerant flag | — | `02010000` | 30 |
| `pressureratioalarm` | Pressure ratio flag | — | `02010000` | 30 |
| `heat_pump_boxalarm` | Heat-pump box flag | — | `02010000` | 31 |
| `filterchangealarm` | Filter change flag | — | `02010000` | 31 |
| `heating_overheatalarm` | Heating overheat flag | — | `02010000` | 31 |
| `te0_sensorfail` | TE0 sensor fault flag | — | `02010000` | 33 |
| `te1_sensorfail` | TE1 sensor fault flag | — | `02010000` | 33 |
| `te2_sensorfail` | TE2 sensor fault flag | — | `02010000` | 33 |
| `te3_sensorfail` | TE3 sensor fault flag | — | `02010000` | 33 |
| `te4_sensorfail` | TE4 sensor fault flag | — | `02010000` | 33 |
| `te5_sensorfail` | TE5 sensor fault flag | — | `02010000` | 33 |
| `te6_sensorfail` | TE6 sensor fault flag | — | `02010000` | 33 |
| `te7_sensorfail` | TE7 sensor fault flag | — | `02010000` | 33 |
| `te24_sensorfail` | TE24 sensor fault flag | — | `02010000` | 36 |
| `fan_mode` | Fan mode | — | `02007a03` | 91 |
| `clock_hours` | Pump hours | — | `02010000` | 4 |
| `clock_minutes` | Pump minutes | — | `02010000` | 5 |
| `clock_seconds` | Pump seconds | — | `02010000` | 6 |
| `clock_day` | Pump day | — | `02010000` | 7 |
| `clock_month` | Pump month | — | `02010000` | 8 |
| `clock_year` | Pump year | — | `02010000` | 9 |
| `fireplace_code` | Fireplace action code | — | `02011602` | 156 |
| `control_mode_code` | Heating control mode code | — | `02000000` | 12 |
| `hot_water_priority_code` | Hot-water priority code | — | `02000000` | 14 |
| `holiday_days` | Holiday reduction | days | `02001602` | 140 |
| `curve_10` | Heating curve at +10°C | °C | `02016401` | 124 |
| `curve_5` | Heating curve at +5°C | °C | `02016401` | 126 |
| `curve_0` | Heating curve at +0°C | °C | `02016401` | 128 |
| `curve_m5` | Heating curve at -5°C | °C | `02016401` | 130 |
| `curve_m10` | Heating curve at -10°C | °C | `02016401` | 132 |
| `curve_m15` | Heating curve at -15°C | °C | `02016401` | 134 |
| `minimum_return` | Minimum return temperature | °C | `02001602` | 170 |
| `additional_heat_threshold` | Additional heat outdoor threshold | °C | `0200c802` | 104 |
| `filter_days` | Time to filter change | days | `02007a03` | 122 |
| `heating_valve` | Valve directed to heating | — | `02010000` | 26 |
| `hot_water_valve` | Valve directed to hot water | — | `02010000` | 26 |
| `extra_hot_water_code` | Extra hot-water code (0=off, 32=on) | — | `02010000` | 24 |
| `highgaspres_alarmreset` | High gas pressure acknowledgement bit | — | `02010000` | 37 |
| `hot_water_overheatalarmreset` | Hot-water overheat acknowledgement bit | — | `02010000` | 38 |
| `lackofcoolantalarmreset` | Low refrigerant acknowledgement bit | — | `02010000` | 38 |
| `pressureratioalarmreset` | Pressure ratio acknowledgement bit | — | `02010000` | 38 |
| `heat_pump_boxalarmreset` | Heat-pump box acknowledgement bit | — | `02010000` | 39 |
| `heating_overheatalarmreset` | Heating overheat acknowledgement bit | — | `02010000` | 39 |
| `te0_sensorfailreset` | TE0 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te1_sensorfailreset` | TE1 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te2_sensorfailreset` | TE2 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te3_sensorfailreset` | TE3 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te4_sensorfailreset` | TE4 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te5_sensorfailreset` | TE5 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te6_sensorfailreset` | TE6 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te7_sensorfailreset` | TE7 sensor fault acknowledgement bit | — | `02010000` | 41 |
| `te24_sensorfailreset` | TE24 sensor fault acknowledgement bit | — | `02010000` | 44 |
| `filterchangealarmmute` | Filter change acknowledgement bit | — | `02010000` | 39 |
| `compressor_running` | Compressor running (from frequency) | — | derived | — |
| `compressor_power_heating` | Compressor power to heating (derived) | W | derived | — |
| `compressor_power_hot_water` | Compressor power to hot water (derived) | W | derived | — |
| `additional_power_heating` | Additional heat to heating (derived) | W | derived | — |
| `additional_power_hot_water` | Additional heat to hot water (derived) | W | derived | — |
