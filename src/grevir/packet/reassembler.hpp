// Extracted from Ardoinus ardo_packet_reassembler.h; see LICENSE.txt.
#pragma once
#include <grevir/packet/header.hpp>

namespace ardo {
// Address is an opaque, copyable, equality-comparable peer identity.
template <std::uint32_t w_MaxFragmentCount, std::uint32_t w_FragmentSize = 256,
          typename Address = std::uint32_t>
class PacketReassembler {
public:
  using BitmapType = std::uint32_t;
  using StreamSequenceId = ardo::StreamSequenceId;
  using Fragment = packet_detail::Fragment<w_FragmentSize>;
  static constexpr std::uint32_t MAX_FRAGMENT_COUNT = w_MaxFragmentCount;
  static constexpr std::uint32_t FRAGMENT_SIZE = w_FragmentSize;
  static constexpr std::uint32_t MAX_PACKET_SIZE =
    packet_detail::Limits<MAX_FRAGMENT_COUNT, FRAGMENT_SIZE>::capacity;
  static constexpr std::uint32_t LIMIT_MAX_FRAGMENT_COUNT = 32;
  static_assert(offsetof(Fragment, payload) == packet_detail::header_size);

  static constexpr std::uint32_t min_frag_size() {
    return packet_detail::header_size;
  }
  enum ReassemblerState { CLEAR, PARTIAL, COMPLETE };
  struct PacketSequencer { std::uint32_t packet_sequence_gen{}; };
  struct ConsumeStatus { PacketReassembler* most_eligible_node{}; };
  enum MaybeConsumeResult {
    NOT_CONSUMED, NOT_CONSUMED_POSSIBLE_REPEAT,
    CONSUMED_INCOMPLETE, CONSUMED_AND_COMPLETE, INVALID_FRAGMENT
  };

  ReassemblerState state = CLEAR;
  Address remoteIp{};
  StreamSequenceId stream_sequence_id{};
  std::uint32_t current_length{};
  std::uint16_t remotePort{};
  std::uint32_t packet_sequence{};
  BitmapType outstanding_bitmap{};
  std::uint8_t fragment_max_index{};
  std::uint8_t payload[MAX_PACKET_SIZE]{};

  // Inspect bytes, not a Fragment object overlaid onto untyped input storage.
  static bool valid_fragment(const std::uint8_t* data, std::uint32_t length) {
    if (length <= min_frag_size() || length - min_frag_size() > FRAGMENT_SIZE
        || !packet_detail::has_marker(data, length)) {
      return false;
    }
    const auto index = data[12];
    const auto last = data[13];
    return last < MAX_FRAGMENT_COUNT && index <= last
      && (index == last || length - min_frag_size() == FRAGMENT_SIZE);
  }

  bool start_new_packet(const Address& address, std::uint16_t port, std::uint8_t last) {
    if (last >= MAX_FRAGMENT_COUNT) {
      return false;
    }
    state = PARTIAL;
    remoteIp = address;
    remotePort = port;
    fragment_max_index = last;
    current_length = 0;
    const auto count = last + 1u;
    outstanding_bitmap = count == LIMIT_MAX_FRAGMENT_COUNT
      ? std::numeric_limits<BitmapType>::max() : (BitmapType(1) << count) - BitmapType(1);
    return true;
  }

  bool unset_outstanding_bit(std::uint8_t bit) {
    if (state != PARTIAL || bit > fragment_max_index) {
      return false;
    }
    outstanding_bitmap &= ~(BitmapType(1) << bit);
    if (outstanding_bitmap == 0) {
      state = COMPLETE;
      return true;
    }
    return false;
  }

  bool apply_fragment(const std::uint8_t* data, const PacketSequencer& sequencer,
                      std::uint32_t length) {
    if (!valid_fragment(data, length) || state != PARTIAL
        || data[13] != fragment_max_index) {
      return false;
    }
    StreamSequenceId incoming{};
    ::memcpy(incoming.stream_sequence, data + 4, 8);
    if (!(incoming == stream_sequence_id)) {
      return false;
    }
    const auto index = data[12];
    if ((outstanding_bitmap & (BitmapType(1) << index)) == 0) {
      return false; // First received copy wins; duplicates cannot overwrite it.
    }
    packet_sequence = sequencer.packet_sequence_gen;
    const auto offset = FRAGMENT_SIZE * index;
    const auto size = length - min_frag_size();
    ::memcpy(payload + offset, data + min_frag_size(), size);
    if (index == fragment_max_index) {
      current_length = offset + size;
    }
    return unset_outstanding_bit(index);
  }

  bool consume(const PacketSequencer& sequencer, const Address& address,
               std::uint16_t port, const std::uint8_t* data, std::uint32_t length) {
    if (!valid_fragment(data, length)) {
      return false;
    }
    start_new_packet(address, port, data[13]);
    ::memcpy(stream_sequence_id.stream_sequence, data + 4, 8);
    return apply_fragment(data, sequencer, length);
  }

  MaybeConsumeResult maybe_consume(ConsumeStatus& status, const PacketSequencer& sequencer,
      const Address& address, std::uint16_t port, const std::uint8_t* data,
      std::uint32_t length) {
    if (!valid_fragment(data, length)) {
      return INVALID_FRAGMENT;
    }
    StreamSequenceId incoming{};
    ::memcpy(incoming.stream_sequence, data + 4, 8);
    if (state != CLEAR && packet_matches(address, port, incoming)) {
      if (data[13] != fragment_max_index) {
        return INVALID_FRAGMENT;
      }
      if (state == COMPLETE) {
        return NOT_CONSUMED_POSSIBLE_REPEAT;
      }
      return apply_fragment(data, sequencer, length)
        ? CONSUMED_AND_COMPLETE : CONSUMED_INCOMPLETE;
    }
    pick_most_eligible(status, sequencer);
    return NOT_CONSUMED;
  }

  bool packet_matches(const Address& address, std::uint16_t port,
                      const StreamSequenceId& id) const {
    return address == remoteIp && port == remotePort && id == stream_sequence_id;
  }
  bool contains_undelivered_data() const {
    return state == PARTIAL;
  }

  // Prefer empty slots, then completed slots, then the least recently advanced partial.
  void pick_most_eligible(ConsumeStatus& status, const PacketSequencer& sequencer) {
    const auto rank = [](ReassemblerState value) {
      return value == CLEAR ? 0 : (value == COMPLETE ? 1 : 2);
    };
    const auto selected = status.most_eligible_node;
    if (!selected || rank(state) < rank(selected->state)
        || (rank(state) == rank(selected->state)
            && sequencer.packet_sequence_gen - packet_sequence
              > sequencer.packet_sequence_gen - selected->packet_sequence)) {
      status.most_eligible_node = this;
    }
  }
};
} // namespace ardo
