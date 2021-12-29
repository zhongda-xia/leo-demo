/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#ifndef SAT_CONSTELLATION_H
#define SAT_CONSTELLATION_H

#include "../common.hpp"

#include "ns3/waypoint.h"

#include "ns3/point-to-point-net-device.h"

#include <string>
#include <vector>
#include <map>

namespace ns3 {
namespace ndn {
namespace sat {

using std::string;
using std::vector;
using std::map;
using std::pair;

struct satellite {
  string name;
  Ptr<Node> node;
  string satPrefix;
  Ptr<PointToPointNetDevice> p2pDevice;
  string rvPrefix;
  satellite()
    : p2pDevice(nullptr)
  {
  }
};

struct station {
  string name;
  Ptr<Node> node;
  bool isHost;
  string role;
  vector<string> prefixes;
  vector<string> consumerStNames;
  vector<Ptr<Application>> consumerApps;
  Ptr<PointToPointNetDevice> p2pDevice;
  Ptr<PointToPointNetDevice> lastP2pDevice;
  vector<pair<int, string>> attachments;
  size_t curAttachmentIdx;
  size_t lastAttachmentIdx;
  bool handover;
  station()
    : isHost(false)
    , role("")
    , p2pDevice(nullptr)
    , lastP2pDevice(nullptr)
    , curAttachmentIdx(0)
    , lastAttachmentIdx(0)
    , handover(false)
  {
  }
};

extern bool sameOrbit;
struct UpdateParams {
    int interval;
    int curTime; // in minutes
    int period; // in ms
};

map<string, vector<string>>
readCsv(string filename);

vector<string>
split(string s, string delimiter);

void
ShowShimOverhead(string path);

shared_ptr<::nfd::face::Face>
SatPointToPointNetDeviceCallback(Ptr<Node> node, Ptr<L3Protocol> ndn, Ptr<NetDevice> device);

void
Update(UpdateParams params, map<string, satellite>* pSatellites, map<string, station>* pStations,
       map<pair<string, string>, vector<pair<int, vector<string>>>>* pRoutes,
       map<string, vector<pair<int, map<string, vector<pair<string, string>>>>>>* pProducerRoutes);

} // namespace sat
} // namespace ndn
} // namespace ns3

#endif