// Adapted from Ardoinus ardOnet/tests/PaketReassemblerTest.cxx; see LICENSE.txt.
#include <GrevirPacket.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <random>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
using Packets = std::vector<Bytes>;
Bytes payload(unsigned size) {
  Bytes result{65, 66, 67, 68};
  for (unsigned i = 0; i < size; ++i) {
    result.push_back(static_cast<std::uint8_t>(size + i));
  }
  return result;
}
template <typename Sender>
Packets fragments(Sender& sender, const Bytes& bytes) {
  Packets result;
  REQUIRE(sender.send(bytes.data(), static_cast<std::uint32_t>(bytes.size()),
    [&](const std::uint8_t* data, std::uint32_t length) {
      result.emplace_back(data, data + length);
    }));
  return result;
}
template <typename Receiver, typename Address = std::uint32_t>
void deliver(Receiver& receiver, const Bytes& bytes, Packets& completed,
             Address address = Address{1}, std::uint16_t port = 42) {
  receiver.receive(address, port, bytes.data(), static_cast<std::uint32_t>(bytes.size()),
    [&](const std::uint8_t* data, std::uint32_t length) {
      completed.emplace_back(data, data + length);
    });
}
} // namespace

TEST_CASE("legacy stream sequence IDs copy and compare") {
  using Id = ardo::PacketReassembler<10, 100>::StreamSequenceId;
  Id a{{1, 2, 3}};
  Id b{{4, 5, 6}};
  REQUIRE_FALSE(a == b);
  b = a;
  REQUIRE(a == b);
}

TEST_CASE("legacy fragmented payload sizes round trip") {
  ardo::FragmentSender<20, 20> sender;
  ardo::FragmentReceiver<1, 20, 20> receiver;
  for (const auto size : {21u, 350u, 396u}) {
    const auto original = payload(size);
    const auto sent = fragments(sender, original);
    REQUIRE(sent.size() >= 2);
    Packets completed;
    for (const auto& packet : sent) {
      deliver(receiver, packet, completed);
    }
    REQUIRE(completed == Packets{original});
  }
}

TEST_CASE("legacy interleaved packets reassemble in deterministic shuffled orders") {
  for (const auto sizes : {std::vector<unsigned>{300}, {301}, {99, 300, 301}}) {
    for (unsigned seed = 0; seed < 32; ++seed) {
      ardo::FragmentSender<20, 74> sender;
      ardo::FragmentReceiver<4, 20, 74> receiver;
      Packets expected, sent, completed;
      for (const auto size : sizes) {
        expected.push_back(payload(size));
        const auto packet = fragments(sender, expected.back());
        sent.insert(sent.end(), packet.begin(), packet.end());
      }
      std::mt19937 random(seed);
      std::shuffle(sent.begin(), sent.end(), random);
      for (const auto& packet : sent) {
        deliver(receiver, packet, completed);
      }
      std::sort(expected.begin(), expected.end());
      std::sort(completed.begin(), completed.end());
      REQUIRE(completed == expected);
    }
  }
}

TEST_CASE("fragment bytes retain the legacy marker IDs and little endian CRC") {
  ardo::FragmentSender<4, 4> sender;
  const Bytes data{'1','2','3','4','5','6','7','8','9'};
  REQUIRE(sender.crc32c(0, data.data(), 9) == 0xe3069283u);
  const auto sent = fragments(sender, data);
  REQUIRE(sent.size() == 3);
  REQUIRE(sent[0] == Bytes{0x0f, 'r', 0xaa, 'G', 0x83, 0x92, 0x06, 0xe3,
                           1, 0, 0, 0, 0, 2, '1', '2', '3', '4'});
  REQUIRE(sent[2].size() == 15);
  REQUIRE(sent[2][12] == 2);
  REQUIRE(sent[2][14] == '9');
}

TEST_CASE("all fragment orders and duplicates deliver once without replacing stored bytes") {
  ardo::FragmentSender<4, 4> sender;
  const auto original = payload(8);
  const auto sent = fragments(sender, original);
  std::array<unsigned, 3> order{0, 1, 2};
  do {
    ardo::FragmentReceiver<1, 4, 4> receiver;
    Packets completed;
    for (const auto index : order) {
      deliver(receiver, sent[index], completed);
      auto duplicate = sent[index];
      duplicate.back() ^= 0xff;
      deliver(receiver, duplicate, completed);
    }
    REQUIRE(completed == Packets{original});
    for (const auto& packet : sent) {
      deliver(receiver, packet, completed);
    }
    REQUIRE(completed.size() == 1);
  } while (std::next_permutation(order.begin(), order.end()));
}

TEST_CASE("fragment count changes are rejected without damaging a partial packet") {
  ardo::FragmentSender<4, 4> sender;
  const auto original = payload(8);
  const auto sent = fragments(sender, original);
  ardo::FragmentReceiver<1, 4, 4> receiver;
  Packets completed;
  deliver(receiver, sent[0], completed);
  auto conflict = sent[1];
  conflict[13] = 1;
  deliver(receiver, conflict, completed);
  REQUIRE(receiver.reassembler_manager.error_malformed_packet == 1);
  REQUIRE(completed.empty());
  deliver(receiver, sent[2], completed);
  deliver(receiver, sent[1], completed);
  REQUIRE(completed == Packets{original});
}

TEST_CASE("malformed fragment lengths and indices leave the pool untouched") {
  ardo::FragmentSender<4, 4> sender;
  const auto sent = fragments(sender, payload(8));
  ardo::PacketReassemblerManager<1, 4, 4> manager;
  auto reject = [&](const Bytes& bytes) {
    const auto before = manager.error_malformed_packet;
    REQUIRE(manager.consume_fragment_packet(1, 42, bytes.data(),
      static_cast<std::uint32_t>(bytes.size())) == nullptr);
    REQUIRE(manager.error_malformed_packet == before + 1);
    REQUIRE(manager.packet_Reassemblers[0].state == decltype(manager)::Reassembler::CLEAR);
  };
  for (unsigned size = 0; size <= 14; ++size) {
    reject(Bytes(sent[0].begin(), sent[0].begin() + size));
  }
  auto bad = sent[2];
  bad.push_back(0); // A last fragment may not exceed one fragment's payload capacity.
  reject(bad);
  bad = sent[2];
  bad[12] = 3;
  bad[13] = 3;
  bad.resize(14 + 16); // Legacy total-packet bound allowed a copy beyond the buffer.
  reject(bad);
  bad = sent[0];
  bad.pop_back(); // Non-final fragments must be full.
  reject(bad);
  bad = sent[0];
  bad[12] = 3;
  reject(bad);
  bad = sent[0];
  bad[13] = 4;
  reject(bad);
  bad = sent[0];
  bad[0] = 0;
  reject(bad);
  Bytes large(65536 + sent[0].size());
  std::copy(sent[0].begin(), sent[0].end(), large.begin());
  reject(large); // Must not narrow length to uint16_t before validation.
  REQUIRE(manager.consume_fragment_packet(1, 42, nullptr, 18) == nullptr);
  REQUIRE(manager.packet_Reassemblers[0].state == decltype(manager)::Reassembler::CLEAR);
}

TEST_CASE("full 32-bit bitmap and packets over 65535 bytes reassemble") {
  ardo::FragmentSender<32, 4096> sender;
  ardo::FragmentReceiver<1, 32, 4096> receiver;
  const auto original = payload(32 * 4096 - 4);
  auto sent = fragments(sender, original);
  REQUIRE(sent.size() == 32);
  Packets completed;
  std::reverse(sent.begin(), sent.end());
  for (const auto& packet : sent) {
    deliver(receiver, packet, completed);
  }
  REQUIRE(completed == Packets{original});
  REQUIRE(receiver.reassembler_manager.packet_Reassemblers[0].outstanding_bitmap == 0);
}

TEST_CASE("short raw messages empty payloads and reserved marker messages round trip") {
  ardo::FragmentSender<4, 16> sender;
  ardo::FragmentReceiver<1, 4, 16> receiver;
  for (const Bytes data : {Bytes{1}, Bytes{1, 2, 3}, Bytes{0x0f, 'r', 0xaa, 'G'}, payload(12)}) {
    const auto sent = fragments(sender, data);
    Packets completed;
    for (const auto& packet : sent) {
      deliver(receiver, packet, completed);
    }
    REQUIRE(completed == Packets{data});
  }
  unsigned empty = 0;
  REQUIRE(sender.send(nullptr, 0, [&](const std::uint8_t* data, std::uint32_t length) {
    receiver.receive(1, 42, data, length, [&](const std::uint8_t*, std::uint32_t n) {
      REQUIRE(n == 0);
      ++empty;
    });
  }));
  REQUIRE(empty == 1);
  unsigned calls = 0;
  auto callback = [&](const std::uint8_t*, std::uint32_t) { ++calls; };
  REQUIRE_FALSE(sender.send(nullptr, 1, callback));
  const auto too_big = payload(61);
  REQUIRE_FALSE(sender.send(too_big.data(), static_cast<std::uint32_t>(too_big.size()), callback));
  receiver.receive(1, 42, nullptr, 1, callback);
  REQUIRE(calls == 0);
}

TEST_CASE("address and port identities keep equal packet IDs separate") {
  using Address = std::array<std::uint8_t, 16>;
  ardo::FragmentSender<4, 4> sender;
  ardo::FragmentReceiver<3, 4, 4, Address> receiver;
  const auto original = payload(8);
  const auto sent = fragments(sender, original);
  Packets completed;
  for (const auto& packet : sent) {
    deliver(receiver, packet, completed, Address{1}, 42);
    deliver(receiver, packet, completed, Address{2}, 42);
    deliver(receiver, packet, completed, Address{1}, 43);
  }
  REQUIRE(completed == Packets{original, original, original});
}

TEST_CASE("bounded pool eviction follows recent progress across sequence wrap") {
  ardo::FragmentSender<4, 4> sender;
  ardo::FragmentReceiver<2, 4, 4> receiver;
  auto& manager = receiver.reassembler_manager;
  manager.packet_sequencer.packet_sequence_gen = 0xfffffffdu;
  const auto a = fragments(sender, payload(8));
  const auto b = fragments(sender, payload(7));
  const auto c = fragments(sender, payload(6));
  Packets completed;
  deliver(receiver, a[0], completed); // fffffffe
  deliver(receiver, b[0], completed); // ffffffff
  deliver(receiver, a[1], completed); // 0: A is most recent.
  deliver(receiver, c[0], completed); // 1: evict B.
  deliver(receiver, a[2], completed);
  REQUIRE(completed == Packets{payload(8)});
  for (unsigned i = 1; i < c.size(); ++i) {
    deliver(receiver, c[i], completed);
  }
  REQUIRE(completed == Packets{payload(8), payload(6)});
}

TEST_CASE("ordinary move-only callbacks require no std-function conversion") {
  struct Callback {
    Callback() = default;
    Callback(const Callback&) = delete;
    unsigned calls = 0;
    void operator()(const std::uint8_t*, std::uint32_t) { ++calls; }
  } callback;
  ardo::FragmentSender<4, 4> sender;
  const auto original = payload(8);
  REQUIRE(sender.send(original.data(), static_cast<std::uint32_t>(original.size()), callback));
  REQUIRE(callback.calls == 3);
  ardo::FragmentReceiver<1, 4, 4> receiver;
  const std::uint8_t byte = 1;
  receiver.receive(1, 42, &byte, 1, callback);
  REQUIRE(callback.calls == 4);
}
