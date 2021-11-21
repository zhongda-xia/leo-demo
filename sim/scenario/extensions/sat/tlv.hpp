#ifndef SAT_TLV_HPP
#define SAT_TLV_HPP

namespace ns3 {
namespace ndn {
namespace sat {
namespace tlv {

enum {
  AdaptationPacket = 600,
  PacketType = 601,
  LinkId = 602, // string
  HopLimit = 603, // int
  TunnelId = 604, // string
  Payload = 605
};

} // namespace tlv
} // namespace sat
} // namespace ndn
} // namespace ns3

#endif