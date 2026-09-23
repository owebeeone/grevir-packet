#include <GrevirPacket.h>
#include <array>
#include <cstring>

int main() {
  // Supply an opaque peer identity; no network or board header is required.
  using Address = std::array<std::uint8_t, 4>;
  ardo::FragmentSender<4, 4> sender;
  ardo::FragmentReceiver<2, 4, 4, Address> receiver;
  const std::array<std::uint8_t, 9> input{1, 2, 3, 4, 5, 6, 7, 8, 9};
  unsigned received = 0;
  bool matches = false;
  const bool sent = sender.send(input.data(), input.size(),
    [&](const std::uint8_t* data, std::uint32_t size) {
      receiver.receive(Address{127, 0, 0, 1}, 1234, data, size,
        [&](const std::uint8_t* packet, std::uint32_t length) {
          ++received;
          matches = length == input.size() && std::memcmp(packet, input.data(), length) == 0;
        });
    });
  return sent && received == 1 && matches ? 0 : 1;
}
