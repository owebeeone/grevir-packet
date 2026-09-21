// Extracted from Ardoinus ardo_packet_reassembler.h; see LICENSE.txt.
#pragma once
#include <grevir/packet/reassembler.hpp>
#include <functional>

namespace ardo {
template <std::uint32_t w_MaxFragmentCount, std::uint32_t w_FragmentSize = 256>
class FragmentSender {
public:
  static constexpr std::uint32_t MAX_FRAGMENT_COUNT = w_MaxFragmentCount;
  static constexpr std::uint32_t FRAGMENT_SIZE = w_FragmentSize;
  using Reassembler = PacketReassembler<MAX_FRAGMENT_COUNT, FRAGMENT_SIZE>;
  static constexpr std::uint32_t MAX_PACKET_SIZE = Reassembler::MAX_PACKET_SIZE;
  using SenderFunc = std::function<void(const std::uint8_t*, std::uint32_t)>;
  using Fragment = typename Reassembler::Fragment;
  using StreamSequenceId = typename Reassembler::StreamSequenceId;
  static constexpr std::uint32_t POLY = 0x82f63b78;

  // CRC-32C, inherited bitwise algorithm (reversed iSCSI polynomial).
  // Original attribution: https://stackoverflow.com/questions/27939882/fast-crc-algorithm
  static std::uint32_t crc32c(std::uint32_t crc, const std::uint8_t* data, std::uint32_t length) {
    crc = ~crc;
    while (length--) {
      crc ^= *data++;
      for (std::uint8_t k = 0; k < 8; ++k) {
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
      }
    }
    return ~crc;
  }

  template <typename Sender>
  bool send(const std::uint8_t* data, std::uint32_t length, Sender&& sender) {
    if (length > MAX_PACKET_SIZE || (data == nullptr && length != 0)) {
      return false;
    }
    // Escape the reserved marker even for a short, otherwise unfragmented message.
    if (length <= FRAGMENT_SIZE && !packet_detail::has_marker(data, length)) {
      sender(data, length);
      return true;
    }
    const auto crc = crc32c(0, data, length);
    ++sequence_number;
    Fragment fragment;
    for (unsigned i = 0; i < 4; ++i) {
      fragment.stream_sequence_id.stream_sequence[i] = static_cast<std::uint8_t>(crc >> (8u * i));
      fragment.stream_sequence_id.stream_sequence[i + 4] =
        static_cast<std::uint8_t>(sequence_number >> (8u * i));
    }
    fragment.fragment_max_index = static_cast<std::uint8_t>((length - 1) / FRAGMENT_SIZE);
    std::uint32_t start = 0;
    for (std::uint8_t i = 0; start < length; ++i) {
      fragment.fragment_index = i;
      const auto size = length - start < FRAGMENT_SIZE ? length - start : FRAGMENT_SIZE;
      std::memcpy(fragment.payload, data + start, size);
      sender(reinterpret_cast<const std::uint8_t*>(&fragment), Reassembler::min_frag_size() + size);
      start += size;
    }
    return true;
  }

  std::uint32_t sequence_number{};
};
} // namespace ardo
