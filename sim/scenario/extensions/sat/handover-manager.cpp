/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "handover-manager.hpp"

#include "core.hpp"
#include "tlv.hpp"

#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/model/ndn-block-header.hpp"

#include "ns3/ndnSIM/ndn-cxx/encoding/block-helpers.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/forwarder.hpp"

#include "ns3/channel.h"

#include <string>

NS_LOG_COMPONENT_DEFINE("ndn.sat.HandoverManager");

namespace ns3 {
namespace ndn {
namespace sat {

NS_OBJECT_ENSURE_REGISTERED(HandoverManager);

uint64_t HandoverManager::m_hopLimit = 2;

TypeId
HandoverManager::GetTypeId()
{
  static TypeId tid = TypeId("ns3::ndn::sat::HandoverManager").SetGroupName("Sat").SetParent<Object>().AddConstructor<HandoverManager>()
                      .AddTraceSource("ForwardPayloads", "ForwardPayloads",
                                      MakeTraceSourceAccessor(&HandoverManager::m_forwardPayload),
                                      "ns3::ndn::sat::HandoverManager::PayloadTraceCallback");
  return tid;
}

HandoverManager::HandoverManager()
  : m_inReqs(0)
  , m_outReqs(0)
  , m_inAcks(0)
  , m_outAcks(0)
  , m_inPayloads(0)
  , m_outPayloads(0)
{
}

void
HandoverManager::NotifyNewAggregate()
{
  if (m_ndn == 0) {
    m_ndn = GetObject<L3Protocol>();
  }
  Object::NotifyNewAggregate();
}

Ptr<L3Protocol>
HandoverManager::GetL3Protocol() const
{
  return m_ndn;
}

void
HandoverManager::TunnelPacket(const ::nfd::face::Transport::Packet& packet, string tunnelId)
{
  NS_LOG_DEBUG("Tunnel packet via tunnel " << tunnelId);
  core::LinkPayload payloadPacket(tunnelId, packet.packet);
  // create transport packet based on the LinkPayload packet
  ::nfd::face::Transport::Packet newPacket(Block(payloadPacket.wireEncode()));

  // convert NFD packet to NS3 packet
  BlockHeader header(newPacket); // header is defined to classify NDN-borne packets

  Ptr<Packet> ns3Packet = Create<Packet>();
  ns3Packet->AddHeader(header);

  // send the NS3 packet
  if (m_tib.find(tunnelId) == m_tib.end()) {
    NS_LOG_DEBUG("Tunnel not yet established for " << tunnelId);
    // queue for later transmission
    if (m_dt.find(tunnelId) == m_dt.end()) {
      NS_LOG_DEBUG("Buffer data for " << tunnelId);
      m_dt[tunnelId] = vector<::nfd::face::Transport::Packet>();
    }
    m_dt[tunnelId].push_back(packet);
  }
  else {
    auto nexthop = m_tib[tunnelId].first;
    if (nexthop == nullptr) {
      nexthop = m_tib[tunnelId].second;
    }
    NS_LOG_DEBUG("Tunnelling packet through " << nexthop->GetAddress());
    nexthop->Send(ns3Packet, nexthop->GetBroadcast(), L3Protocol::ETHERNET_FRAME_TYPE);
  }
}

void
HandoverManager::ProcessPacket(const ::nfd::face::Transport::Packet& packet, Ptr<NetDevice> lasthop)
{
  Block wire(packet.packet);
  wire.parse();
  switch (::ndn::encoding::readNonNegativeInteger(wire.get(tlv::PacketType))) {
    case core::Type_LinkPayload: // forward further according to tunnel ID, or hand payload up to NDN
    {
      m_inPayloads++;
      NS_LOG_DEBUG("Processing LinkPayload");
      auto id = ::ndn::encoding::readString(wire.get(tlv::TunnelId));
      auto entry = m_tib.find(id);
      if (entry != m_tib.end()) {
        // proceed if a match is found in TIB, else discard
        auto end0 = std::pair<Ptr<NetDevice>, Ptr<NetDevice>>(nullptr, lasthop);
        auto end1 = std::pair<Ptr<NetDevice>, Ptr<NetDevice>>(lasthop, nullptr);
        if (entry->second == end0 || entry->second == end1) {
          // end of tunnel, hand up payload
          NS_LOG_DEBUG("End of tunnel for " << id << ", handing up payload to NDN");
          auto faceId = m_faceIdTable[id];
          ((UserLinkTransport*)(m_ndn->getFaceById(faceId)->getTransport()))
            ->inject(::nfd::face::Transport::Packet(Block(wire.get(tlv::Payload).blockFromValue())));
        }
        else {
          // traverse tunnel
          m_outPayloads++;
          NS_LOG_DEBUG("Traverse channel " << id);
          auto nexthop = m_tib[id].first;
          if (nexthop == lasthop) {
            nexthop = m_tib[id].second;
          }
          ((UserLinkTransport*)(m_ndn->getFaceByNetDevice(nexthop)->getTransport()))
            ->emit(::nfd::face::Transport::Packet(packet));
          BOOST_ASSERT(wire.find(tlv::Payload) != wire.elements_end());
          auto& payloadBlock = wire.get(tlv::Payload);
          m_forwardPayload(*m_ndn->getFaceByNetDevice(nexthop), payloadBlock);
        }
      }
      else {
        NS_LOG_DEBUG("Data discarded " << id);
      }
      break;
    }
    case core::Type_TunnelReq: // reply ack through lasthop (and mark TIB), or further broadcast
    {
      m_inReqs++;
      NS_LOG_DEBUG("Processing TunnelReq");
      auto id = ::ndn::encoding::readString(wire.get(tlv::LinkId));
      auto it = std::find(m_idList.begin(), m_idList.end(), id);
      if (it != m_idList.end()) {
        // this is the target ground terminal, send ack
        NS_LOG_DEBUG("found sat, send ack for " << id);
        m_tib[id] = std::pair<Ptr<NetDevice>, Ptr<NetDevice>>(nullptr, lasthop); // incoming link of req points to the ground terminal
        // send buffered data
        if (m_dt.find(id) != m_dt.end()) {
          NS_LOG_DEBUG("Send out buffered data for " << id);
          for (auto& packet : m_dt[id]) {
            TunnelPacket(packet, id);
          }
          m_dt.erase(id);
        }
        m_idList.erase(it);
        core::TunnelAck ack(id);
        ::nfd::face::Transport::Packet ackPacket(Block(ack.wireEncode()));
        ((UserLinkTransport*)(m_ndn->getFaceByNetDevice(lasthop)->getTransport()))
          ->emit(::nfd::face::Transport::Packet(ackPacket));
        m_outAcks++;
      }
      else {
        if (m_prt.find(id) != m_prt.end()) {
          NS_LOG_DEBUG("loop detected, discard tReq for " << id);
        }
        else {
          NS_LOG_DEBUG("update prt and broadcast for " << id);
          m_prt[id] = lasthop;
          BroadcastReq(id, ::ndn::encoding::readNonNegativeInteger(wire.get(tlv::HopLimit)), lasthop);
        }
      }
      break;
    }
    case core::Type_TunnelAck: // forward ack according to PRT, and update TIB
    {
      m_inAcks++;
      NS_LOG_DEBUG("Processing TunnelAck");
      auto id = ::ndn::encoding::readString(wire.get(tlv::LinkId));
      if (m_prt.find(id) == m_prt.end()) {
        NS_LOG_DEBUG("Unsolicited or redundant Ack, discard");
        break;
      }
      auto rLasthop = m_prt[id]; // recorded incoming link of corresponding req
      if (rLasthop != nullptr) {
        // not destination, further forward
        NS_LOG_DEBUG("Forward according to PRT for " << id);
        ((UserLinkTransport*)(m_ndn->getFaceByNetDevice(rLasthop)->getTransport()))
          ->emit(::nfd::face::Transport::Packet(packet));
        m_outAcks++;
      }
      NS_LOG_DEBUG("Update TIB");
      m_tib[id] = std::pair<Ptr<NetDevice>, Ptr<NetDevice>>(lasthop, rLasthop); // recorded nexthop is nullptr if this is destination, i.e., ground terminal
      NS_LOG_DEBUG("Purge PRT entry");
      m_prt.erase(id);
      break;
    }
    default:
      NS_LOG_DEBUG("Unrecognized type");
      break;
  }
}

void
HandoverManager::BroadcastReq(string id, uint64_t hopLimit, Ptr<NetDevice> lasthop)
{
  if (hopLimit == 0) {
    // sender of req, set to max hop limit
    hopLimit = m_hopLimit + 1;
  }

  if (hopLimit - 1 == 0) {
    NS_LOG_DEBUG("Reached hop limit: " << id);
    return;
  }
  NS_LOG_DEBUG("Broadcast req for " << id << ", remaining hops " << hopLimit - 1);
  core::TunnelReq req(id, hopLimit - 1);
  ::nfd::face::Transport::Packet packet(Block(req.wireEncode()));
  auto& faces = m_ndn->getForwarder()->getFaceTable();
  for (auto face = faces.begin(); face != faces.end(); face++) {
    if (face->getScope() == ::ndn::nfd::FaceScope::FACE_SCOPE_LOCAL) {
      continue;
    }
    auto transport = (UserLinkTransport*)(face->getTransport());
    if (transport->GetNetDevice() == lasthop || transport->m_isGone) {
      continue;
    }
    NS_LOG_DEBUG("Broadcast through " << transport->GetNetDevice()->GetAddress());
    transport->emit(::nfd::face::Transport::Packet(packet));
    m_outReqs++;
  }
  if (lasthop == nullptr)
    m_prt[id] = nullptr; // mark as destination in PRT
}

void
HandoverManager::AddUserLink(string id, ::nfd::FaceId faceId)
{
  NS_LOG_DEBUG("record user link " << id << " " << faceId);
  m_idList.push_back(id);
  m_faceIdTable[id] = faceId;
}

} // namespace sat
} // namespace ndn
} // namespace ns3
