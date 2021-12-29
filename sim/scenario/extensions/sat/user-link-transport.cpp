/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "user-link-transport.hpp"

#include "tlv.hpp"
#include "handover-manager.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/model/ndn-block-header.hpp"
#include "ns3/ndnSIM/utils/ndn-ns3-packet-tag.hpp"

#include "ns3/ndnSIM/ndn-cxx/encoding/block.hpp"
#include "ns3/ndnSIM/ndn-cxx/interest.hpp"
#include "ns3/ndnSIM/ndn-cxx/data.hpp"

#include "ns3/queue.h"

NS_LOG_COMPONENT_DEFINE("ndn.sat.UserLinkTransport");

namespace ns3 {
namespace ndn {
namespace sat {

class HandoverManager;

bool UserLinkTransport::m_doShim = false;

UserLinkTransport::UserLinkTransport(Ptr<Node> node,
                                     const Ptr<NetDevice>& netDevice,
                                     const string& localUri,
                                     const string& remoteUri,
                                     ::ndn::nfd::FaceScope scope,
                                     ::ndn::nfd::FacePersistency persistency,
                                     ::ndn::nfd::LinkType linkType)
  : NetDeviceTransport(node, netDevice, localUri, remoteUri, false, scope, persistency, linkType)
  , m_isUserLink(false)
  , m_isGone(false)
{
  NS_LOG_FUNCTION(this << "Creating an ndnSIM transport (SIN) instance for netDevice with URI"
                  << this->getLocalUri());

  NS_ASSERT_MSG(m_netDevice != 0, "NetDeviceFace needs to be assigned a valid NetDevice");

  m_node->RegisterProtocolHandler(MakeCallback(&UserLinkTransport::receiveFromNetDevice, this),
                                  L3Protocol::ETHERNET_FRAME_TYPE, m_netDevice,
                                  true /*promiscuous mode*/);
}

UserLinkTransport::~UserLinkTransport()
{
  NS_LOG_FUNCTION_NOARGS();
}

void
UserLinkTransport::emit(Packet&& packet)
{
  NS_LOG_DEBUG("Emitting packet from netDevice with URI" << this->getLocalUri());

  // convert NFD packet to NS3 packet
  BlockHeader header(packet);

  Ptr<ns3::Packet> ns3Packet = Create<ns3::Packet>();
  ns3Packet->AddHeader(header);

  // send the NS3 packet
  m_netDevice->Send(ns3Packet, m_netDevice->GetBroadcast(),
                    L3Protocol::ETHERNET_FRAME_TYPE);
}


void
UserLinkTransport::inject(Packet&& packet)
{
  NS_LOG_DEBUG("Injecting packet to netDevice with URI" << this->getLocalUri());

  this->receive(std::move(packet));
}

void
UserLinkTransport::doSend(Packet&& packet)
{
  NS_LOG_DEBUG("Sending packet from netDevice with URI " << this->getLocalUri());

  if (m_isGone) {
    if (m_doShim) {
      NS_LOG_DEBUG("Tunnelling packet from face whose netDevice URI is " << this->getLocalUri());
      // m_id is the identifier of the user link (which should also be the tunnel ID), and associated with the corresponding face (stored in the transport)
      m_node->GetObject<HandoverManager>()->TunnelPacket(packet, m_id);
    }
    else {
      NS_LOG_DEBUG("Link broken, discard packet because shim layer mechanisms are disabled");
    }
    return;
  }

  // convert NFD packet to NS3 packet
  BlockHeader header(packet);

  Ptr<ns3::Packet> ns3Packet = Create<ns3::Packet>();
  ns3Packet->AddHeader(header);

  // send the NS3 packet
  m_netDevice->Send(ns3Packet, m_netDevice->GetBroadcast(),
                    L3Protocol::ETHERNET_FRAME_TYPE);
}

// callback
void
UserLinkTransport::receiveFromNetDevice(Ptr<NetDevice> device,
                                        Ptr<const ns3::Packet> p,
                                        uint16_t protocol,
                                        const Address& from, const Address& to,
                                        NetDevice::PacketType packetType)
{
  NS_LOG_FUNCTION(device << p << protocol << from << to << packetType);

  // Convert NS3 packet to NFD packet
  Ptr<ns3::Packet> packet = p->Copy();

  BlockHeader header;
  packet->RemoveHeader(header);

  auto nfdPacket = Packet(std::move(header.getBlock()));

  if (nfdPacket.packet.type() == tlv::AdaptationPacket) {
    if (m_doShim) {
      NS_LOG_DEBUG("Received adaptation layer packet");
      m_node->GetObject<HandoverManager>()->ProcessPacket(nfdPacket, device); // device is lasthop
    }
    else {
      NS_LOG_DEBUG("Adaptation layer packet received, but shim layer mechanisms are disabled");
    }
  }
  else {
    this->receive(std::move(nfdPacket));
  }
}

} // namespace sat
} // namespace ndn
} // namespace ns3
