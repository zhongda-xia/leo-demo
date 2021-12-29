/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#ifndef SAT_USER_LINK_TRANSPORT_HPP
#define SAT_USER_LINK_TRANSPORT_HPP

#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/model/ndn-net-device-transport.hpp"

#include "ns3/net-device.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/pointer.h"

#include "ns3/point-to-point-net-device.h"
#include "ns3/channel.h"

namespace ns3 {
namespace ndn {
namespace sat {

using std::string;

class UserLinkTransport : public NetDeviceTransport
{
public:
  static bool m_doShim; // controls whether the shim layer mechanims are enabled

  UserLinkTransport(Ptr<Node> node, const Ptr<NetDevice>& netDevice,
                     const string& localUri,
                     const string& remoteUri,
                     ::ndn::nfd::FaceScope scope = ::ndn::nfd::FACE_SCOPE_NON_LOCAL,
                     ::ndn::nfd::FacePersistency persistency = ::ndn::nfd::FACE_PERSISTENCY_PERSISTENT,
                     ::ndn::nfd::LinkType linkType = ::ndn::nfd::LINK_TYPE_POINT_TO_POINT);

  ~UserLinkTransport();

  void
  emit(Packet&& packet); // send

  void
  inject(Packet&& packet); // receive

private:
  virtual void
  doSend(Packet&& packet) override;

  void
  receiveFromNetDevice(Ptr<NetDevice> device,
                       Ptr<const ns3::Packet> p,
                       uint16_t protocol,
                       const Address& from, const Address& to,
                       NetDevice::PacketType packetType);

public:
  bool m_isUserLink;
  bool m_isGone;
  string m_id;
};

} // namespace sat
} // namespace ndn
} // namespace ns3

#endif
