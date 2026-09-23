// Extracted from Ardoinus ardo_packet_reassembler.h; see LICENSE.txt.
#pragma once
#include <grevir/packet/reassembler.hpp>
#include <grevir/base/compat/array.hpp>

namespace ardo {
template <std::uint32_t w_InFlightPacketCount, std::uint32_t w_MaxFragmentCount,
          std::uint32_t w_FragmentSize = 256, typename Address = std::uint32_t>
class PacketReassemblerManager {
public:
  static constexpr std::uint32_t IN_FLIGHT_PACKET_COUNT = w_InFlightPacketCount;
  static constexpr std::uint32_t MAX_FRAGMENT_COUNT = w_MaxFragmentCount;
  static constexpr std::uint32_t FRAGMENT_SIZE = w_FragmentSize;
  using Reassembler = PacketReassembler<MAX_FRAGMENT_COUNT, FRAGMENT_SIZE, Address>;
  static constexpr std::uint32_t MAX_PACKET_SIZE = Reassembler::MAX_PACKET_SIZE;
  using ConsumeStatus = typename Reassembler::ConsumeStatus;
  using PacketSequencer = typename Reassembler::PacketSequencer;
  using Fragment = typename Reassembler::Fragment;
  static_assert(IN_FLIGHT_PACKET_COUNT > 0, "At least one in-flight packet slot is required");
  static_assert(IN_FLIGHT_PACKET_COUNT <= (std::numeric_limits<std::size_t>::max)() / sizeof(Reassembler),
                "Reassembler pool must fit the target address space");

  Reassembler* consume_fragment_packet(const Address& address, std::uint16_t port,
                                      const std::uint8_t* data, std::uint32_t length) {
    if (!Reassembler::valid_fragment(data, length)) {
      ++error_malformed_packet;
      return nullptr;
    }
    ConsumeStatus status{};
    ++packet_sequencer.packet_sequence_gen;
    for (auto& node : packet_Reassemblers) {
      const auto result = node.maybe_consume(status, packet_sequencer, address, port, data, length);
      switch (result) {
        case Reassembler::NOT_CONSUMED: {
          break;
        }
        case Reassembler::INVALID_FRAGMENT: {
          ++error_malformed_packet;
          return nullptr;
        }
        case Reassembler::NOT_CONSUMED_POSSIBLE_REPEAT:
        case Reassembler::CONSUMED_INCOMPLETE: {
          return nullptr;
        }
        case Reassembler::CONSUMED_AND_COMPLETE: {
          return &node;
        }
      }
    }
    auto* node = status.most_eligible_node;
    return node->consume(packet_sequencer, address, port, data, length) ? node : nullptr;
  }

  std::uint32_t error_malformed_packet{};
  PacketSequencer packet_sequencer{};
  std::array<Reassembler, IN_FLIGHT_PACKET_COUNT> packet_Reassemblers{};
};

template <std::uint32_t w_InFlightPacketCount, std::uint32_t w_MaxFragmentCount,
          std::uint32_t w_FragmentSize = 256, typename Address = std::uint32_t>
class FragmentReceiver {
public:
  static constexpr std::uint32_t IN_FLIGHT_PACKET_COUNT = w_InFlightPacketCount;
  static constexpr std::uint32_t MAX_FRAGMENT_COUNT = w_MaxFragmentCount;
  static constexpr std::uint32_t FRAGMENT_SIZE = w_FragmentSize;
  using ReassemblerManager = PacketReassemblerManager<IN_FLIGHT_PACKET_COUNT,
    MAX_FRAGMENT_COUNT, FRAGMENT_SIZE, Address>;
  using Reassembler = typename ReassemblerManager::Reassembler;
  static constexpr std::uint32_t MAX_PACKET_SIZE = Reassembler::MAX_PACKET_SIZE;

  // Callbacks are invoked synchronously; ordinary lambdas need no type-erased storage.
  template <typename Receiver>
  void receive(const Address& address, std::uint16_t port, const std::uint8_t* data,
               std::uint32_t length, Receiver&& receiver) {
    if (data == nullptr && length != 0) {
      ++reassembler_manager.error_malformed_packet;
      return;
    }
    if (!packet_detail::has_marker(data, length)) {
      receiver(data, length);
      return;
    }
    auto* node = reassembler_manager.consume_fragment_packet(address, port, data, length);
    if (node) {
      receiver(node->payload, node->current_length);
    }
  }
  ReassemblerManager reassembler_manager{};
};
} // namespace ardo
