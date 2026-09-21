# Grevir Packet

Transport-independent packet fragmentation and reassembly, extracted from Ardoinus
`ardOnet/src/ardo_packet_reassembler.h`. The package supplies header format,
reassembler, bounded pool/receiver and sender headers under `grevir/packet/`.
`GrevirPacket.h` includes them all. It retains the `ardo` names and has no other
Grevir, Arduino, Wi-Fi or UDP dependency.

## Installed use

```cmake
find_package(grevir-packet CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE grevir::packet)
```

The target requests C++23. The implementation uses standard `<array>`, `<cstddef>`,
`<cstdint>`, `<cstring>`, `<limits>` and `<functional>` headers. The latter retains
the optional `SenderFunc`/`ReceiverFunc` aliases. Ordinary callbacks are passed as
template arguments, with no implicit `std::function` construction or storage.
A caller may still explicitly choose an allocating callback or address type.

```cpp
#include <GrevirPacket.h>

using Address = std::array<std::uint8_t, 4>;
ardo::FragmentSender<8, 256> sender;
ardo::FragmentReceiver<4, 8, 256, Address> receiver;
```

The receiver's last template argument is the peer address type; it defaults to an
opaque uint32 identity. It must support default construction, copying, assignment
and equality. The codec does not interpret its bytes or network byte order.
A board integration can explicitly supply its own `IPAddress` type. Address, port
and stream ID together identify a reassembly. See the
[installed loopback consumer](tests/installed-consumer/main.cpp) for a complete
example without any transport headers.

## Storage and callback contract

- Fragment count is 1–32; payload capacity per fragment is positive. Each receiver
  slot retains one `count * fragment_size` payload array plus metadata. The number
  of slots is fixed and positive. The sender uses one wire fragment on the stack.
  Capacities must fit uint32 lengths and the target's `size_t` address space;
  users must select capacities appropriate to their available RAM.
- `send(data, length, callback)` returns false for an oversized message or a null
  pointer with nonzero length. Otherwise it invokes the callback synchronously
  for each wire packet. Success means the callbacks ran, not transport delivery.
  Empty messages are passed through. Input length must describe valid readable
  storage; neither sender nor receiver can verify the allocation behind a pointer.
- Receive callbacks are synchronous. Copy bytes that need to survive the callback;
  outgoing fragment storage is reused immediately, and completed receive buffers
  may be reused for subsequent packets. Do not reenter the same sender/receiver
  while its callback is active or access it concurrently without external control.
- A pool uses empty slots first, then completed slots, then the partial packet with
  the least recent new-fragment progress. Duplicate fragments do not refresh that
  progress. Sequence-age comparison handles uint32 wraparound provided a retained
  slot does not span a full 2^32 validated-fragment arrivals.
- Completed IDs suppress duplicate delivery only while their slot remains retained.
  Eviction loses that history. There is no timeout, acknowledgement or retransmission
  protocol; transport integrations must provide any such policy.

## Wire format and validation

The existing 14-byte prefix is preserved: four marker bytes `0f 72 aa 47`, eight
stream-ID bytes, fragment index and last-fragment index. Stream IDs contain a
little-endian CRC-32C followed by the sender's little-endian uint32 sequence number.
Both sides must agree on fragment payload capacity.

Unmarked packets pass through unchanged. Messages at or below fragment capacity
normally pass through, too; a short message beginning with the reserved marker is
now wrapped as a single fragment so it cannot be misread as framing. Fragmented
payloads must be nonempty, every non-final fragment must be full, and the final
fragment must fit one fragment capacity. Header, length, indices and an unchanged
last-fragment index are checked before payload copies. The receiver reads byte
fields without overlaying a C++ object onto input storage.

Fragments may arrive out of order. The first accepted copy of each index wins;
repeated copies cannot overwrite stored payload bytes. The CRC remains an identifier,
as in the original implementation: it is **not verified on reassembly**, so the
codec does not promise payload-corruption detection. No CRC verification or wire
protocol version was added during extraction.

## Corrections and validation

Inherited issues corrected here include the hidden Arduino address dependency,
invalid narrowing in stream-ID construction, signed/native-int bitmap shifts,
oversized-final-fragment copies, inconsistent fragment counts, uint16 length
truncation, duplicate overwrites, the marker collision and wraparound-sensitive
buffer selection. Raw untyped input is parsed as bytes. The legacy harness had
missing transport types, invalid callback captures/signatures and a main function
that discarded failures; its payload/ID/reordering scenarios are now Catch2 cases
with reproducible shuffle seeds.

Twelve packet cases and all 148 workspace host cases pass. Coverage includes the
original message sizes and shuffled streams, exact wire bytes and a known CRC-32C
vector, all permutations of a three-fragment message, duplicates, malformed input,
peer separation, the full 32-bit bitmap, a 128-KiB packet, marker escaping, empty
messages, sequence wrap and callbacks that cannot be copied into `std::function`.
Five public headers compile independently. One valid and six expected-rejection
compiler probes check configuration bounds. Raw-token scope checks cover all eight
new production/test C++ files, including any inactive preprocessor sections.

The same twelve cases pass address/undefined sanitizers. A separate production
build/install/consumer run uses only this package, with Catch2 and Test Support
discovery disabled. Host results do not establish MCU library availability, RAM
fit, code size or timing. Source review includes the shared policy and conventional
AVR integer promotions; AVR compiler/hardware validation remain on hold.

Standalone tests require installed development-only Grevir Test Support plus
Catch2 3.8.1; production and consumer builds require neither. Arduino metadata is
provided, but Arduino compilation has not been validated.
