// Extracted from Ardoinus ardo_packet_reassembler.h; see LICENSE.txt.
#pragma once
#include <grevir/base/compat/cstddef.hpp>
#include <grevir/base/compat/cstdint.hpp>
#include <grevir/base/compat/limits.hpp>
#include <string.h>

namespace ardo {
struct HeaderContainer {
  std::uint8_t header[4];
  bool operator==(const HeaderContainer& rhs) const {
    return ::memcmp(header, rhs.header, sizeof(header)) == 0;
  }
  bool operator!=(const HeaderContainer& rhs) const {
    return !(*this == rhs);
  }
};

struct FragmentHeaderContainer {
  const HeaderContainer header_container{{0x0f, 'r', 0xaa, 'G'}};
  bool operator==(const FragmentHeaderContainer& rhs) const {
    return header_container == rhs.header_container;
  }
  bool operator!=(const FragmentHeaderContainer& rhs) const {
    return !(*this == rhs);
  }
};

struct StreamSequenceId {
  std::uint8_t stream_sequence[8]{};
  bool operator==(const StreamSequenceId& rhs) const {
    return ::memcmp(stream_sequence, rhs.stream_sequence, sizeof(stream_sequence)) == 0;
  }
};

namespace packet_detail {
inline constexpr std::uint32_t header_size = 14;
inline bool has_marker(const std::uint8_t* data, std::uint32_t length) {
  const FragmentHeaderContainer marker{};
  return data != nullptr && length >= 4
    && ::memcmp(data, marker.header_container.header, 4) == 0;
}

template <std::uint32_t Count, std::uint32_t Size>
struct Limits {
  static_assert(Count > 0 && Count <= 32, "Fragment count must be in 1..32");
  static_assert(Size > 0, "Fragment size must be positive");
  static_assert(Count == 0 || Size <= (std::numeric_limits<std::uint32_t>::max)() / Count,
                "Packet capacity must fit uint32_t");
  static_assert(Size <= (std::numeric_limits<std::uint32_t>::max)() - header_size,
                "Wire fragment length must fit uint32_t");
  static constexpr std::uint32_t capacity = Count * Size;
  static_assert(capacity <= (std::numeric_limits<std::size_t>::max)(),
                "Packet buffer must fit the target address space");
  static_assert(Size <= (std::numeric_limits<std::size_t>::max)() - header_size,
                "Wire fragment must fit the target address space");
};

template <std::uint32_t Size>
struct Fragment {
  FragmentHeaderContainer fragment_header;
  StreamSequenceId stream_sequence_id;
  std::uint8_t fragment_index{};
  std::uint8_t fragment_max_index{};
  std::uint8_t payload[Size];
};
} // namespace packet_detail
} // namespace ardo
