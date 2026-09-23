#include <GrevirPacket.h>
#include <string.h>

ardo::FragmentSender<2, 8> packet_sender;
ardo::FragmentReceiver<1, 2, 8> packet_receiver;
volatile unsigned char packet_status = 0;

void setup() {
  const std::uint8_t payload[9]{1, 2, 3, 4, 5, 6, 7, 8, 9};
  unsigned char deliveries = 0;
  const bool sent = packet_sender.send(payload, sizeof(payload),
    [&](const std::uint8_t* frame, std::uint32_t frame_length) {
      packet_receiver.receive(std::uint32_t{1}, 1234, frame, frame_length,
        [&](const std::uint8_t* data, std::uint32_t length) {
          ++deliveries;
          if (length == sizeof(payload) && ::memcmp(data, payload, sizeof(payload)) == 0) {
            packet_status = 1;
          }
        });
    });
  if (!sent || deliveries != 1) {
    packet_status = 2;
  }
}

void loop() {
}
