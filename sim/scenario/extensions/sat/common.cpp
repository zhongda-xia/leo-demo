/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "common.hpp"

#include <cmath>
#include <sstream>
#include <fstream>

#include "ns3/log.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-helper.h"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/NFD/daemon/face/generic-link-service.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/strategy.hpp"

#include "../kite/apps/producer/producer-app.hpp"

#include "user-link-transport.hpp"
#include "overlay-manager.hpp"
#include "apps/consumer/consumer.hpp"
#include "apps/consumer/consumer-cbr.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.sat.common");

namespace ns3 {
namespace ndn {
namespace sat {

using std::pow;
using std::sqrt;
using std::sin;

// declarations
 
string
constructFaceUri(Ptr<NetDevice> netDevice);

std::shared_ptr<nfd::Face>
createAndRegisterFace(Ptr<Node> node, Ptr<L3Protocol> ndn, Ptr<NetDevice> device);

void
AttachPrefix(satellite& sat, station& station);

void
DetachPrefix(station& station, satellite& sat);

void
ApplyRoutes(vector<string> prefixes, vector<string> links, map<string, satellite>& satellites);

void
RemoveRoutes(vector<string> prefixes, vector<string> links, map<string, satellite>& satellites);

void
UpdateRoutes(vector<string> prefixes, map<string, vector<pair<string, string>>>& routes, map<string, satellite>& satellites);

void
OverlayResend(Ptr<Node> stationNode, string tunnelId);

// public

uint64_t timeTillBreak = 100;

map<string, vector<string>>
readCsv(string filename)
{
  map<string, vector<string>> result;

  std::ifstream csvFile(filename);

  string line, colName;
  map<int, string> colIdxMap;

  // read column names
  if (csvFile.good()) {
      std::getline(csvFile, line);
      stringstream ss(line);
      int index = 0;
      while(std::getline(ss, colName, ',')) {
        result[colName] = vector<string>{};
        colIdxMap[index] = colName;
        index++;
      }
  }
  else {
    return result;
  }

  string val;
  // read values line by line
  while (std::getline(csvFile, line)) {
      stringstream ss(line);
      int colIdx = 0;
      while (std::getline(ss, val, ',')) {
        result[colIdxMap[colIdx]].push_back(val);
        colIdx++;
      }
  }

  csvFile.close();

  return result;
}

vector<string>
split(string s, string delimiter) {
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  string token;
  vector<string> res;
  while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
  }
  res.push_back(s.substr(pos_start));
  return res;
}

void
ShowOverlayCount(string path)
{
  std::ofstream os;
  os.open(path.c_str(), std::ios_base::out | std::ios_base::trunc);
  // if (!os->is_open()) {
  //   NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
  //   return;
  // }

  os << "Node\tInReq\tOutReq\tInAck\tOutAck\tInPayload\tOutPayload\n";
  for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++) {
    auto overlayManager = (*node)->GetObject<OverlayManager>();
    os << Names::FindName(*node) << "\t" << overlayManager->m_inReqs << "\t" << overlayManager->m_outReqs << "\t" << overlayManager->m_inAcks << "\t" << overlayManager->m_outAcks << "\t" << overlayManager->m_inPayloads << "\t" << overlayManager->m_outPayloads << "\n";
  }
  os.close();
}

shared_ptr<::nfd::face::Face>
SatPointToPointNetDeviceCallback(Ptr<Node> node, Ptr<L3Protocol> ndn, Ptr<NetDevice> device)
{
  NS_LOG_INFO("Creating point-to-point (SIN) Face on node " << node->GetId());

  Ptr<PointToPointNetDevice> netDevice = DynamicCast<PointToPointNetDevice>(device);
  NS_ASSERT(netDevice != nullptr);

  // access the other end of the link
  Ptr<PointToPointChannel> channel = DynamicCast<PointToPointChannel>(netDevice->GetChannel());
  NS_ASSERT(channel != nullptr);

  Ptr<NetDevice> remoteNetDevice = channel->GetDevice(0);
  if (remoteNetDevice->GetNode() == node)
    remoteNetDevice = channel->GetDevice(1);

  // Create an ndnSIM-specific transport instance
  ::nfd::face::GenericLinkService::Options opts;
  opts.allowFragmentation = true;
  opts.allowReassembly = true;
  opts.allowCongestionMarking = true;

  auto linkService = make_unique<::nfd::face::GenericLinkService>(opts);

  auto transport = make_unique<UserLinkTransport>(node, netDevice,
                                                  constructFaceUri(netDevice),
                                                  constructFaceUri(remoteNetDevice));

  auto face = std::make_shared<::nfd::face::Face>(std::move(linkService), std::move(transport));
  face->setMetric(1);

  ndn->addFace(face);
  NS_LOG_LOGIC("Node " << node->GetId() << ": added Face as face #"
                       << face->getLocalUri());

  return face;
}

void
Update(UpdateParams params, map<string, satellite>* pSatellites, map<string, station>* pStations,
       map<pair<string, string>, vector<pair<int, vector<string>>>>* pRoutes,
       map<string, vector<pair<int, map<string, vector<pair<string, string>>>>>>* pProducerRoutes)
{
  NS_LOG_INFO("Update: " << params.curTime << "min");
  auto interval = params.interval;
  auto curTime = params.curTime;
  params.curTime += interval;

  auto& satellites = *pSatellites;
  auto& stations = *pStations;

  for (auto& item : stations) { // traverse active stations
    auto& station = item.second;
    if (!station.isHost) {
      continue;
    }

    if (station.curAttachmentIdx >= station.attachments.size()) {
      continue;
    }

    string oldId = "";
    if (station.handover) {
      if (station.p2pDevice) { // attached to a satellite the last time
        auto oldStFace = station.node->GetObject<L3Protocol>()->getFaceByNetDevice(station.p2pDevice);
        BOOST_ASSERT(oldStFace);

        // store previous user link id
        oldId = ((UserLinkTransport*)(oldStFace->getTransport()))->m_id;

        // remove faces for the last last user link
        // BOOST_ASSERT(station.p2pDevice->GetChannel()->GetNDevices() == 2);
        // auto oldSatDevice = station.p2pDevice->GetChannel()->GetDevice(0);
        // if (oldSatDevice == station.p2pDevice) {
        //   oldSatDevice = station.p2pDevice->GetChannel()->GetDevice(1);
        // }
        // BOOST_ASSERT(oldSatDevice);
        // auto oldSatFace = oldSatDevice->GetNode()->GetObject<L3Protocol>()->getFaceByNetDevice(oldSatDevice);
        // BOOST_ASSERT(oldSatFace);
        // oldStFace->close();
        // oldSatFace->close();

        // remove previous local routes
        auto lastSatName = station.attachments[station.lastAttachmentIdx].second;
        auto& oldSat = satellites[lastSatName];
        DetachPrefix(station, oldSat);

        // finally reset p2p device
        station.p2pDevice = nullptr;
        oldSat.p2pDevice = nullptr;
      }
    }

    auto curSatName = station.attachments[station.curAttachmentIdx].second;
    if (curSatName == "-") { // no sat within range
      NS_LOG_INFO("No sat within range for " << station.name);
    }
    else if (!station.handover && curTime > 0) {
      NS_LOG_INFO("Same sat for " << station.name);
    }
    else {
      auto& sat = satellites[curSatName];

      // TODO: update user link delay

      // TODO: assert no active p2p user link

      // create new p2p link, and store devices
      PointToPointHelper p2pHelper;
      auto devices = p2pHelper.Install(station.node, sat.node);
      station.p2pDevice = devices.Get(0)->GetObject<PointToPointNetDevice>();
      sat.p2pDevice = devices.Get(1)->GetObject<PointToPointNetDevice>();
      NS_LOG_INFO("Connect " << station.name << " " << station.node->GetId() << " to " << sat.name << " " << sat.node->GetId());
    
      // update NDN faces, install default routes
      NS_LOG_INFO("Update NDN faces on " << station.name);
      createAndRegisterFace(station.node, station.node->GetObject<L3Protocol>(), station.p2pDevice);
      NS_LOG_INFO("Update NDN faces on " << sat.name);
      createAndRegisterFace(sat.node, sat.node->GetObject<L3Protocol>(), sat.p2pDevice);

      stringstream ss;
      ss << station.p2pDevice->GetAddress() << "-" << sat.p2pDevice->GetAddress();
      string userLinkId = ss.str();
      NS_LOG_INFO("Set user link ID to " << userLinkId << ", for " << station.name);
      auto stFace = station.node->GetObject<L3Protocol>()->getFaceByNetDevice(station.p2pDevice);
      ((UserLinkTransport*)(stFace->getTransport()))->m_isUserLink = true;
      ((UserLinkTransport*)(stFace->getTransport()))->m_id = userLinkId;
      station.node->GetObject<OverlayManager>()->AddUserLink(userLinkId, stFace->getId());

      NS_LOG_INFO("Set user link ID to " << userLinkId << ", for " << sat.name);
      auto satFace = sat.node->GetObject<L3Protocol>()->getFaceByNetDevice(sat.p2pDevice);
      ((UserLinkTransport*)(sat.node->GetObject<L3Protocol>()->getFaceByNetDevice(sat.p2pDevice)->getTransport()))->m_isUserLink = true;
      ((UserLinkTransport*)(sat.node->GetObject<L3Protocol>()->getFaceByNetDevice(sat.p2pDevice)->getTransport()))->m_id = userLinkId;
      sat.node->GetObject<OverlayManager>()->AddUserLink(userLinkId, satFace->getId());

      // attach prefix to access satellite by adding route to station
      AttachPrefix(sat, station);

      // send t-req if overlay enabled and attachment changes
      if (oldId == "") {
        NS_LOG_INFO("No previous attachment, do not send req");
      }
      else if (station.role == "consumer" || station.role == "m_producer") {
        NS_LOG_INFO("Mobile consumer or producer, send T-Req if overlay is enabled");
        if (UserLinkTransport::m_doOverlay) {
          NS_LOG_INFO("Broadcast req for " << oldId);
          station.node->GetObject<OverlayManager>()->BroadcastReq(oldId, 0, nullptr);
          if (station.role == "consumer") {
            NS_LOG_INFO("Mobile consumer, schedule resend after T-Req timeout");
            // schedule retransmit upon anticipation of failure (ISL delay uniformly set to 10ms), add 5ms for what?
            Simulator::Schedule (MilliSeconds (OverlayManager::m_hopLimit*10*2+5), &OverlayResend, station.node, oldId);
          }
        }
        else {
          NS_LOG_INFO("Overlay disabled, do not send req");
        }
      }

      if (station.role == "m_producer") {
        // send kite update everytime attachment changes
        NS_LOG_INFO("Mobile producer, initiate KITE update");
        ::ns3::ndn::kite::Producer* producerApp =
          dynamic_cast<::ns3::ndn::kite::Producer*>(&(*(station.node)->GetApplication(0)));
        // 1ms delay to ensure FIBs are up-to-date
        Simulator::Schedule (Seconds (0.001), &::ns3::ndn::kite::Producer::OnAssociation, producerApp);
      }

      NS_LOG_INFO("Attached " << station.name << " to " << sat.name);
    }
  }

  // update manual routes
  for (auto& item : *pRoutes) {
    if (item.second.empty()) {
      NS_LOG_INFO("No more routes from " << item.first.first << " to " << item.first.second);
      continue;
    }
    auto producerSt = stations[item.first.second];
    if (item.second[0].first == 0) {
      // the first time, only add routes
      item.second[0].first = -1; // mark
      ApplyRoutes(producerSt.prefixes, item.second[0].second, satellites);
      NS_LOG_INFO("Added initial routes from " << item.first.first << " to " << item.first.second);
    }
    else if (item.second.size() >= 2) {
      // determine latest route, clear last route
      if (curTime <= item.second[0].first) { // not the time to update routes yet
        NS_LOG_INFO("Same routes (not yet) from " << item.first.first << " to " << item.first.second);
        continue;
      }
      size_t curIdx = 1;
      auto nextTime = item.second[1].first;
      while ((curTime > nextTime) && (curIdx < item.second.size())) {
        curIdx++;
        nextTime = item.second[curIdx].first;
      }
      if (curIdx >= item.second.size()) {
        NS_LOG_INFO("No more routes (exhausted) from " << item.first.first << " to " << item.first.second);
        item.second.clear();
      }
      else {
        bool updateRoute = false;
        if (curTime < nextTime && curIdx != 1) {
          curIdx--;
          updateRoute = true;
          NS_LOG_INFO("Update routes (gap) from " << item.first.first << " to " << item.first.second);
        }
        else if (curTime == nextTime) {
          updateRoute = true;
          NS_LOG_INFO("Update routes (hit) from " << item.first.first << " to " << item.first.second);
        }
      
        if (updateRoute) {
          RemoveRoutes(producerSt.prefixes, item.second[0].second, satellites);
          ApplyRoutes(producerSt.prefixes, item.second[curIdx].second, satellites);
          // pop out stale routes
          item.second.erase(item.second.begin(), item.second.begin()+curIdx-1);
        }
        else {
          NS_LOG_INFO("Same routes (gap) from " << item.first.first << " to " << item.first.second);
        }
      }
    }
    // clear station pairs with no new routes
    if (item.second.size() < 2) {
      item.second.clear();
    }
  }

  for (auto& item : *pProducerRoutes) {
    if (item.second.empty()) {
      NS_LOG_INFO("No more routes from " << item.first);
      continue;
    }
    auto station = stations[item.first];
    size_t nextIdx = 0;
    auto nextTime = item.second[nextIdx].first;
    while (curTime > nextTime && nextIdx < item.second.size()) {
      nextIdx++;
      nextTime = item.second[nextIdx].first;
    }
    bool update = false;
    if (curTime == nextTime) {
      update = true;
    }
    else if (curTime < nextTime) {
      if (nextIdx > 0) {
        update = true;
        nextIdx--;
      }
    }
    else {
      item.second.clear();
    }

    if (update) {
      UpdateRoutes(station.prefixes, item.second[nextIdx].second, satellites);
      item.second.erase(item.second.begin(), item.second.begin()+nextIdx);
    }
  }

  // update attachments (do not actually update links) and schedule consumer transmission if handover occurs
  curTime += interval; // predict the attachment at next update
  for (auto& item : stations) {
    NS_LOG_INFO("Update attachment for " << item.first);
    auto& station = item.second;
    if (!station.isHost) {
      continue;
    }

    if (curTime <= station.attachments[station.curAttachmentIdx].first || station.curAttachmentIdx >= station.attachments.size()) {
      // no need to update attachment, either next handover is yet to happen, or there are no more handovers
      station.handover = false;
      continue;
    }

    auto nextTime = station.attachments[station.curAttachmentIdx].first;
    auto lastAttachmentIdx = station.curAttachmentIdx;
    while (curTime > nextTime) {
      station.curAttachmentIdx++;
      if (station.curAttachmentIdx >= station.attachments.size()) {
        break;
      }
      nextTime = station.attachments[station.curAttachmentIdx].first;
    }

    if (curTime < nextTime) {
      // attachment at curAttachmentIdx-1
      station.curAttachmentIdx--;
      if (station.curAttachmentIdx != lastAttachmentIdx) {
        // handover happens
        station.lastAttachmentIdx = lastAttachmentIdx;
        station.handover = true;
      }
      else {
        station.handover = false;
      }
    }
    else if (curTime == nextTime) {
      // attachment at curAttachmentIdx
      station.lastAttachmentIdx = lastAttachmentIdx;
      station.handover = true;
    }
    else {
      station.handover = false;
    }

    if (station.handover) {
      auto curSatName = station.attachments[station.curAttachmentIdx].second;
      auto lastSatName = station.attachments[station.lastAttachmentIdx].second;
      NS_LOG_INFO("Handover will happen for " << station.name << ", from " << lastSatName << " to " << curSatName);
      if ((curSatName != "-") && (lastSatName != "-")) {
        if (station.role == "consumer") {
          // Simulator::Schedule (MilliSeconds (interval*60*1000-timeTillBreak), &Consumer::SendNewInterest, (Consumer *)&(*(station.node->GetApplication(0))));
          Simulator::Schedule (MilliSeconds (interval*60*1000-params.period), &ConsumerCbr::Resume, (ConsumerCbr *)&(*(station.node->GetApplication(0))));
          Simulator::Schedule (MilliSeconds (interval*60*1000+params.period), &ConsumerCbr::Pause, (ConsumerCbr *)&(*(station.node->GetApplication(0))));

          // update last sat prefix
          Name topPrefix("/sat");
          auto& strategy = station.node->GetObject<L3Protocol>()->getForwarder()->getStrategyChoice().findEffectiveStrategy(topPrefix);
          strategy.m_lastSatPrefix = Name(satellites[lastSatName].satPrefix);
        }
        else if (station.role == "m_producer") {
          for (auto app : station.consumerApps) {
            // Simulator::Schedule (MilliSeconds (interval*60*1000-timeTillBreak), &Consumer::SendNewInterest, (Consumer *)&(*app));
          }
          // also schedule a KITE update immediately to allow the scheduled consumer Interest to reach the previous access satellite
          ::ns3::ndn::kite::Producer* producerApp =
            dynamic_cast<::ns3::ndn::kite::Producer*>(&(*(station.node)->GetApplication(0)));
          Simulator::Schedule (Seconds (0.001), &::ns3::ndn::kite::Producer::OnAssociation, producerApp);
        }
      }
    }
  }

  Simulator::Schedule (Seconds (interval*60), &Update, params, pSatellites, pStations, pRoutes, pProducerRoutes);
}

// private

string
constructFaceUri(Ptr<NetDevice> netDevice)
{
  string uri = "netdev://";
  Address address = netDevice->GetAddress();
  if (Mac48Address::IsMatchingType(address)) {
    uri += "[" + boost::lexical_cast<string>(Mac48Address::ConvertFrom(address)) + "]";
  }

  return uri;
}

std::shared_ptr<nfd::Face>
createAndRegisterFace(Ptr<Node> node, Ptr<L3Protocol> ndn, Ptr<NetDevice> device)
{
  std::shared_ptr<nfd::Face> face = SatPointToPointNetDeviceCallback(node, ndn, device);
  FibHelper::AddRoute(node, "/", face, std::numeric_limits<int32_t>::max());
  return face;
}

void
AttachPrefix(satellite& sat, station& station)
{
  auto satN = sat.node;
  auto stationN = station.node;

  auto& prefixList = station.prefixes;
  if (prefixList.size() > 0) {
    NS_LOG_INFO("Add sat to station route, node " << satN->GetId() << ", sat name: " << sat.name << ", station name: " << station.name);
    for (auto prefix : prefixList) {
      NS_LOG_INFO("-prefix: " << prefix);
      // add route for station prefix
      FibHelper::AddRoute(satN, prefix, satN->GetObject<L3Protocol>()->getFaceByNetDevice(sat.p2pDevice), 1);
    }
  }
  else {
    NS_LOG_INFO("Attached with no prefix, node " << satN->GetId() << ", sat name: " << sat.name << ", station name: " << station.name);
  }
}

void
DetachPrefix(station& station, satellite& sat) {
  auto stFace = station.node->GetObject<L3Protocol>()->getFaceByNetDevice(station.p2pDevice);
  ((UserLinkTransport*)(stFace->getTransport()))->m_isGone = true; // mark gone
  FibHelper::RemoveRoute(station.node, Name("/"), stFace->getId());

  auto satFace = sat.node->GetObject<L3Protocol>()->getFaceByNetDevice(sat.p2pDevice);
  ((UserLinkTransport*)(satFace->getTransport()))->m_isGone = true; // mark gone
  FibHelper::RemoveRoute(sat.node, Name("/"), satFace->getId());

  // remove local route to station
  auto& prefixList = station.prefixes;
  if (prefixList.size() > 0) {
    for (auto prefix : prefixList) {
      FibHelper::RemoveRoute(sat.node, prefix, satFace->getId());
    }
    NS_LOG_INFO("Removed sat to station route, node " << sat.node->GetId() << ", sat name: " << sat.name << ", station name: " << station.name);
  }
}

void
ApplyRoutes(vector<string> prefixes, vector<string> links, map<string, satellite>& satellites)
{
  BOOST_ASSERT(links.size() > 1);
  for (size_t i = 1; i < links.size(); i++) {
    auto node1 = satellites[links[i-1]].node;
    auto node2 = satellites[links[i]].node;
    for (auto prefix : prefixes) {
      FibHelper::AddRoute(node1, prefix, node2, std::numeric_limits<int32_t>::max());
      NS_LOG_INFO("Apply route for " << prefix << " from " << links[i-1] << " " << node1->GetId() << " to " << links[i] << " " << node2->GetId());
    }
  }
}

void
RemoveRoutes(vector<string> prefixes, vector<string> links, map<string, satellite>& satellites)
{
  BOOST_ASSERT(links.size() > 1);
  for (size_t i = 1; i < links.size(); i++) {
    auto node1 = satellites[links[i-1]].node;
    auto node2 = satellites[links[i]].node;
    for (auto prefix : prefixes) {
      FibHelper::RemoveRoute(node1, prefix, node2);
      NS_LOG_INFO("Remove route for " << prefix << " from " << links[i-1] << " to " << links[i]);
    }
  }
}

void
UpdateRoutes(vector<string> prefixes, map<string, vector<pair<string, string>>>& routes, map<string, satellite>& satellites)
{
  for (auto& item : routes["add"]) {
    auto node1 = satellites[item.first].node;
    auto node2 = satellites[item.second].node;
    for (auto prefix : prefixes) {
      FibHelper::AddRoute(node1, prefix, node2, std::numeric_limits<int32_t>::max());
      NS_LOG_INFO("Apply route for " << prefix << " from " << item.first << " " << node1->GetId() << " to " << item.second << " " << node2->GetId());
    }
  }

  for (auto& item : routes["remove"]) {
    auto node1 = satellites[item.first].node;
    auto node2 = satellites[item.second].node;
    for (auto prefix : prefixes) {
      FibHelper::RemoveRoute(node1, prefix, node2);
      NS_LOG_INFO("Remove route for " << prefix << " from " << item.first << " " << node1->GetId() << " to " << item.second << " " << node2->GetId());
    }
  }
}

void
OverlayResend(Ptr<Node> stationNode, string tunnelId)
{
  auto overlayManager = stationNode->GetObject<OverlayManager>();
  if(!overlayManager->isInTib(tunnelId)) {
    NS_LOG_INFO("Tunnel not established after timeout, resend Interest");
    // let forwarder resend all pending Interests
  }
}

} // namespace sat
} // namespace ndn
} // namespace ns3
