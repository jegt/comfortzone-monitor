# Control protocol vectors

The supported write registers and byte order are documented in [the protocol reference](../../docs/protocol.md#supported-writes).

`controls_test.cpp` includes fixed packet vectors for all three fan modes and both extra-hot-water directions, including independently stored checksum bytes. They check exact packet construction. Production commands learn their routing from live request/reply traffic; example addresses in these vectors are not fixed device addresses to transmit.

`end-poll.txt` is a synthetic `02050000` request/reply pair with invented routing and an empty counter payload. It exercises paired route learning and quiet-gap gating. The tests also cover validation, queue exclusion, header/address/CRC rejection, acknowledgement plus fresh read-back, lost acknowledgements, no automatic retry, update interruption and timer rollover.

Write registers are distinct from read-back blocks. Extra hot water writes a two-byte operation value, while read-back is code 0 or 32. Reply codes are raw protocol values; zero is not a universal success code.
