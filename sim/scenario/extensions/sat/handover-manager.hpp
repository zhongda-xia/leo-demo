/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#ifndef SAT_OVERLAY_MANAGER_H
#define SAT_OVERLAY_MANAGER_H

#include "user-link-transport.hpp"

#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/net-device.h"

#include <string>
#include <map>
#include <vector>

namespace ns3{
namespace ndn{
namespace sat{

using std::string;
using std::map;
using std::pair;
using std::vector;

using ::ndn::Block;

typedef map<string, Ptr<NetDevice>> PtrTable;
typedef map<string, pair<Ptr<NetDevice>, Ptr<NetDevice>>> TibTable;
typedef map<string, nfd::FaceId> FaceIdTable;
typedef vector<string> IdList;

typedef map<string, vector<::nfd::face::Transport::Packet>> DataTable;

class HandoverManager : public Object {
public:
  static uint64_t m_hopLimit; // global hop limit

  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static TypeId
  GetTypeId();

  /**
   * @brief Default constructor
   */
  HandoverManager();

  /**
   * @brief Helper function to get smart pointer to ndn::L3Protocol object (basically, self)
   */
  Ptr<L3Protocol>
  GetL3Protocol() const;

  void
  TunnelPacket(const nfd::face::Transport::Packet& packet, string tunnelId);

  void
  ProcessPacket(const nfd::face::Transport::Packet& packet, Ptr<NetDevice> lasthop);

  void
  BroadcastReq(string id, uint64_t hopLimit, Ptr<NetDevice> lasthop);

protected:
  virtual void
  NotifyNewAggregate(); ///< @brief Notify when the object is aggregated to another object (e.g.,
                        /// Node)

public:
  void
  AddUserLink(string id, nfd::FaceId faceId);

  bool
  isInTib(string tunnelId)
  {
    return !(m_tib.find(tunnelId) == m_tib.end());
  }

public:
  uint64_t m_inReqs;
  uint64_t m_outReqs;
  uint64_t m_inAcks;
  uint64_t m_outAcks;
  uint64_t m_inPayloads;
  uint64_t m_outPayloads;

private:
  Ptr<L3Protocol> m_ndn;

  IdList m_idList;

  PtrTable m_prt;
  TibTable m_tib;
  FaceIdTable m_faceIdTable;

  DataTable m_dt;

  TracedCallback<const nfd::Face&, const Block&> m_forwardPayload; ///< @brief trace of outgoing payload
};

} // namespace sat
} // namespace ndn
} // namespace ns3

#endif // SAT_OVERLAY_MANAGER_H
