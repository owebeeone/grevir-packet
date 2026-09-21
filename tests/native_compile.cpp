#include <GrevirPacket.h>
#include <array>
template class ardo::PacketReassembler<32, 16>;
template class ardo::PacketReassemblerManager<4, 8>;
template class ardo::FragmentReceiver<2, 4, 32, std::array<std::uint8_t, 16>>;
template class ardo::FragmentSender<32, 16>;
static_assert(ardo::PacketReassembler<32, 16>::min_frag_size() == 14);
static_assert(ardo::PacketReassembler<32, 16>::MAX_PACKET_SIZE == 512);
