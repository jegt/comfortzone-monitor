# Version 1.1 parameter map

Offsets are relative to the block payload, starting at frame byte 21. Divisor is applied to little-endian numeric values. The symbolic parameter IDs are descriptive identifiers, not register addresses to send on the bus; the four-byte block column is the on-wire identifier. This reference contains 71 entries; the firmware exposes 70 direct fields plus five derived fields.

`HWExtrareached` is not exposed: its offset-zero entry is not a usable standalone measurement. Fields with custom encoding require the rules in [measurements](measurements.md), including year, bit masks, fan mode, priority and extra hot water. Flags and acknowledgement bits are separate values. Sensor fault checks and the API's derived values are implemented in `comfortzone_wifi/measurements.h`.

| Parameter | Internal ID | Block | Payload offset | Divisor | Type |
|---|---:|---|---:|---:|---|
| Highgaspresalarm | 14 | `02010000` | 29 | 1 | OneByteBitPatternX01 |
| HWOverheatalarm | 19 | `02010000` | 30 | 1 | OneByteBitPatternX01 |
| Lackofcoolantalarm | 20 | `02010000` | 30 | 1 | OneByteBitPatternX02 |
| Pressureratioalarm | 24 | `02010000` | 30 | 1 | OneByteBitPatternX20 |
| UnknownHPboxalarm | 27 | `02010000` | 31 | 1 | OneByteBitPatternX01 |
| Filterchangealarm | 28 | `02010000` | 31 | 1 | OneByteBitPatternX02 |
| CWOverheatalarm | 30 | `02010000` | 31 | 1 | OneByteBitPatternX80 |
| TE0Sensorfail | 39 | `02010000` | 33 | 1 | OneByteBitPatternX01 |
| TE1Sensorfail | 40 | `02010000` | 33 | 1 | OneByteBitPatternX02 |
| TE2Sensorfail | 41 | `02010000` | 33 | 1 | OneByteBitPatternX04 |
| TE3Sensorfail | 42 | `02010000` | 33 | 1 | OneByteBitPatternX08 |
| TE4Sensorfail | 43 | `02010000` | 33 | 1 | OneByteBitPatternX10 |
| TE5Sensorfail | 44 | `02010000` | 33 | 1 | OneByteBitPatternX20 |
| TE6Sensorfail | 45 | `02010000` | 33 | 1 | OneByteBitPatternX40 |
| TE7Sensorfail | 46 | `02010000` | 33 | 1 | OneByteBitPatternX80 |
| TE24Sensorfail | 50 | `02010000` | 36 | 1 | OneByteBitPatternX01 |
| Fanstate | 83 | `02007a03` | 91 | 1 | OneByteUnsigned |
| CWCompressorfreq | 90 | `0201c802` | 10 | 10 | TwoBytesSigned |
| Compressorpower | 120 | `0201c802` | 12 | 1 | TwoBytesSigned |
| Hours | 608 | `02010000` | 4 | 1 | OneByteUnsigned |
| Minutes | 609 | `02010000` | 5 | 1 | OneByteUnsigned |
| Seconds | 610 | `02010000` | 6 | 1 | OneByteUnsigned |
| Day | 611 | `02010000` | 7 | 1 | OneByteUnsigned |
| Month | 612 | `02010000` | 8 | 1 | OneByteUnsigned |
| YearAdd2000 | 613 | `02010000` | 9 | 1 | OneByteUnsigned |
| Fireplaceactionon | 614 | `02011602` | 156 | 1 | TwoBytesUnsigned |
| CWControlmode | 616 | `02000000` | 12 | 1 | OneByteUnsigned |
| HWPriority | 617 | `02000000` | 14 | 1 | OneByteUnsigned |
| CWHolidaytime | 629 | `02001602` | 140 | 1 | TwoBytesUnsigned |
| CWHeatingcurve | 630 | `0201c802` | 108 | 1 | OneByteUnsigned |
| CWRoomsetting | 631 | `02016401` | 166 | 10 | TwoBytesSigned |
| CWAdjustwhenout10 | 633 | `02016401` | 124 | 10 | TwoBytesSigned |
| CWAdjustwhenout5 | 634 | `02016401` | 126 | 10 | TwoBytesSigned |
| CWAdjustwhenout0 | 635 | `02016401` | 128 | 10 | TwoBytesSigned |
| CWAdjustwhenoutm5 | 636 | `02016401` | 130 | 10 | TwoBytesSigned |
| CWAdjustwhenoutm10 | 637 | `02016401` | 132 | 10 | TwoBytesSigned |
| CWAdjustwhenoutm15 | 638 | `02016401` | 134 | 10 | TwoBytesSigned |
| CWMinimumFlowReturn | 641 | `02001602` | 170 | 10 | TwoBytesSigned |
| CWHEAllowed | 645 | `0200c802` | 104 | 10 | TwoBytesSigned |
| HWSetting | 663 | `0200c802` | 171 | 10 | TwoBytesSigned |
| Filterchangeperiod | 679 | `02007a03` | 122 | 1 | TwoBytesUnsigned |
| CWOn | 682 | `02010000` | 26 | 1 | OneByteBitPatternX04Inverted |
| HWOn | 683 | `02010000` | 26 | 1 | OneByteBitPatternX04 |
| HWExtra | 684 | `02010000` | 24 | 1 | OneByteUnsigned |
| HWExtrareached | 685 | `02010000` | 0 | 1 | OneByteUnsigned |
| TE0Outdoortemp | 686 | `02010000` | 46 | 10 | TwoBytesSigned |
| TE24Hotwater | 687 | `02010000` | 94 | 10 | TwoBytesSigned |
| Addpower | 711 | `0201c802` | 14 | 1 | TwoBytesUnsigned |
| TE1Flowwater | 727 | `02010000` | 48 | 10 | TwoBytesSigned |
| TE2Returnwater | 728 | `02010000` | 50 | 10 | TwoBytesSigned |
| TE3Indoortemp | 729 | `02010000` | 52 | 10 | TwoBytesSigned |
| TE4Hotgastemp | 730 | `02010000` | 54 | 10 | TwoBytesSigned |
| TE5Exchangerout | 731 | `02010000` | 56 | 10 | TwoBytesSigned |
| TE6Evaporatorin | 732 | `02010000` | 58 | 10 | TwoBytesSigned |
| TE7Exhaustair | 733 | `02010000` | 60 | 10 | TwoBytesSigned |
| HighgaspresAlarmreset | 745 | `02010000` | 37 | 1 | OneByteBitPatternX01 |
| HWOverheatalarmreset | 750 | `02010000` | 38 | 1 | OneByteBitPatternX01 |
| Lackofcoolantalarmreset | 751 | `02010000` | 38 | 1 | OneByteBitPatternX02 |
| Pressureratioalarmreset | 755 | `02010000` | 38 | 1 | OneByteBitPatternX20 |
| UnknownHPboxalarmreset | 758 | `02010000` | 39 | 1 | OneByteBitPatternX01 |
| CWOverheatalarmreset | 761 | `02010000` | 39 | 1 | OneByteBitPatternX80 |
| TE0Sensorfailreset | 770 | `02010000` | 41 | 1 | OneByteBitPatternX01 |
| TE1Sensorfailreset | 771 | `02010000` | 41 | 1 | OneByteBitPatternX02 |
| TE2Sensorfailreset | 772 | `02010000` | 41 | 1 | OneByteBitPatternX04 |
| TE3Sensorfailreset | 773 | `02010000` | 41 | 1 | OneByteBitPatternX08 |
| TE4Sensorfailreset | 774 | `02010000` | 41 | 1 | OneByteBitPatternX10 |
| TE5Sensorfailreset | 775 | `02010000` | 41 | 1 | OneByteBitPatternX20 |
| TE6Sensorfailreset | 776 | `02010000` | 41 | 1 | OneByteBitPatternX40 |
| TE7Sensorfailreset | 777 | `02010000` | 41 | 1 | OneByteBitPatternX80 |
| TE24Sensorfailreset | 781 | `02010000` | 44 | 1 | OneByteBitPatternX01 |
| LMFilterchangealarmmute | 10018 | `02010000` | 39 | 1 | OneByteBitPatternX02 |
