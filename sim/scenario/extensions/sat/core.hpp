#ifndef SAT_CORE_HPP
#define SAT_CORE_HPP

#include "tlv.hpp"

#include "ns3/ndnSIM/ndn-cxx/encoding/block.hpp"
#include "ns3/ndnSIM/ndn-cxx/encoding/block-helpers.hpp"

#include <string>

namespace ns3 {
namespace ndn {
namespace sat {
namespace core {

using ::ndn::Block;
using ::ndn::encoding::makeNonNegativeIntegerBlock;
using ::ndn::encoding::makeStringBlock;
using ::ndn::encoding::makeNonNegativeIntegerBlock;

using std::string;

enum PacketType {
    Type_LinkPayload,
    Type_TunnelReq,
    Type_TunnelAck
};

class TunnelReq {
public:
    static const uint64_t HOP_LIMIT = 2;

    TunnelReq(string linkId)
        : m_linkId(linkId)
        , m_hopLimit(HOP_LIMIT)
        , m_wire(tlv::AdaptationPacket)
    {
    }

    TunnelReq(string linkId, uint64_t hopLimit)
        : m_linkId(linkId)
        , m_wire(tlv::AdaptationPacket)
    {
        // m_hopLimit = (hopLimit < HOP_LIMIT) ? hopLimit : HOP_LIMIT;
        m_hopLimit = hopLimit;
    }

    const Block&
    wireEncode()
    {
        m_wire.push_back(makeNonNegativeIntegerBlock(tlv::PacketType, core::Type_TunnelReq));
        m_wire.push_back(makeStringBlock(tlv::LinkId, m_linkId));
        m_wire.push_back(makeNonNegativeIntegerBlock(tlv::HopLimit, m_hopLimit));
        m_wire.encode();
        return m_wire;
    }

private:
    string m_linkId;
    uint64_t m_hopLimit;
    Block m_wire;
};

class TunnelAck {
public:
    TunnelAck(string linkId)
        : m_linkId(linkId)
        , m_wire(tlv::AdaptationPacket)
    {
    }

    const Block&
    wireEncode()
    {
        m_wire.push_back(makeNonNegativeIntegerBlock(tlv::PacketType, core::Type_TunnelAck));
        m_wire.push_back(makeStringBlock(tlv::LinkId, m_linkId));
        m_wire.encode();
        return m_wire;
    }

private:
    string m_linkId;
    Block m_wire;
};

class LinkPayload {
public:
    LinkPayload(string tunnelId, const Block& payload)
        : m_tunnelId(tunnelId)
        , m_payload(payload)
        , m_wire(tlv::AdaptationPacket)
    {
    }

    const Block&
    wireEncode()
    {
        m_wire.push_back(makeNonNegativeIntegerBlock(tlv::PacketType, core::Type_LinkPayload));
        m_wire.push_back(makeStringBlock(tlv::TunnelId, m_tunnelId));
        Block payloadBlock(tlv::Payload);
        payloadBlock.push_back(m_payload);
        m_wire.push_back(payloadBlock);
        m_wire.encode();
        return m_wire;
    }

private:
    string m_tunnelId;
    Block m_payload;
    Block m_wire;
};

} // core
} // sat
} // ndn
} // ns3

#endif