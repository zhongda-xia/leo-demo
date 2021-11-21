/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "common.hpp"
#include "constellation.hpp"
#include "constellation-p2p.hpp"
#include "overlay/sin-global-routing-helper.hpp"
#include "overlay/sin-overlay-manager.hpp"
#include "overlay/sin-user-link-transport.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <experimental/filesystem>

#include "ns3/object-factory.h"
#include "ns3/point-to-point-net-device.h"

#include "overlay/sin-app-delay-tracer.hpp"
#include "overlay/sin-l3-traffic-tracer.hpp"

NS_LOG_COMPONENT_DEFINE("sin.simple.p2p.kite");

namespace ns3 {

int
main(int argc, char* argv[])
{
  ns3::PacketMetadata::Enable();

  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms")); // constant delay for any ISL
  Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("10000p"));

  CommandLine cmd;

  uint32_t run = 1;
  cmd.AddValue("run", "Run", run);

  int stopTime = 100;
  cmd.AddValue("stop", "stop time", stopTime);

  string resPrefix = "default-";
  cmd.AddValue("resPrefix", "prefix for result files", resPrefix);

  string dataDir = "/Users/ben/Work/kite-sin/data";
  cmd.AddValue("dataDir", "path to data files", dataDir);

  // NDN params
  string strategy = "multicast";
  cmd.AddValue("strategy", "the forwarding strategy to use", strategy);

  float consumerCbrFreq = 1.0;
  cmd.AddValue("consumerCbrFreq", "", consumerCbrFreq);

  string interestLifetime = "2s";
  cmd.AddValue("interestLifetime", "lifetime of consumer Interest", interestLifetime);

  // SIN params
  int updateInterval = 1;
  cmd.AddValue("updateInterval", "interval between link change checks", updateInterval);

  int step = 10;
  cmd.AddValue("step", "satellite trace step time", step);

  string constellationName = "iridium";
  cmd.AddValue("constellation", "constellation", constellationName);

  string stationName = "grid-12x6";
  cmd.AddValue("station", "station", stationName);

  bool sameOrbit = true;
  cmd.AddValue("sameOrbit", "whether always choose sat in the same orbit", sameOrbit);

  double elevationAngle = 10;
  cmd.AddValue("elevationAngle", "minimum elevation angle", elevationAngle);

  bool doOverlay = false;
  cmd.AddValue("doOverlay", "enable overlay", doOverlay);

  uint64_t hopLimit = 2;
  cmd.AddValue("hopLimit", "hop limit", hopLimit);

  uint64_t timeTillBreak = 100;
  cmd.AddValue("timeTillBreak", "time difference between Interest sending and user link breaking", timeTillBreak);

  bool mock = false;
  cmd.AddValue("mock", "mock", mock);

  // kite
  uint32_t traceLifetime = 2000;
  cmd.AddValue("traceLifetime", "trace lifetime (ms)", traceLifetime);
  uint32_t refreshInterval = 2000;
  cmd.AddValue("refreshInterval", "refresh interval (ms)", refreshInterval);
  bool doPull = false;
  cmd.AddValue("doPull", "enable pulling", doPull);

  cmd.Parse(argc, argv);

  Config::SetGlobal("RngRun", UintegerValue(run));

  sin::UserLinkTransport::m_doOverlay = doOverlay;
  sin::OverlayManager::m_hopLimit = hopLimit;
  sin::p2p::timeTillBreak = timeTillBreak;
  sin::p2p::sameOrbit = sameOrbit;

  auto period = sin::getSatPeriod(sin::ORBIT_HEIGHTS.at(constellationName));
  auto maxDistance = sin::getMaxDistance(sin::ORBIT_HEIGHTS.at(constellationName), elevationAngle);
  NS_LOG_UNCOND("Constellation: " << constellationName << ", height: " << sin::ORBIT_HEIGHTS.at(constellationName)
                << ", period: " << period << ", max distance: " << maxDistance);

  stopTime = std::min(stopTime, int(period)); // run for at most an entire orbit period
  int range = int(maxDistance);

  Config::SetDefault("ns3::ndn::L3Protocol::DoPull", BooleanValue(doPull));

  ShowProgress(updateInterval, std::chrono::system_clock::now());

  // read LEO constellation, set up mobility
  NodeContainer satNodes;
  map<string, sin::satellite> satellites;
  string constellation_dir = dataDir + "/constellation/" + constellationName;
  for (const auto & entry : std::experimental::filesystem::directory_iterator(constellation_dir)) {
    string filename = sin::split(entry.path(), "/").back();
    sin::satellite sat;
    sat.path = entry.path();
    sat.name = sin::split(filename, ".").front();
    std::ifstream inFile(sat.path, ios::in);
    string lineStr;
    vector<vector<string>> strArray;
    int time = 0;
    while (getline(inFile, lineStr))
    {
      stringstream ss(lineStr);
      string str;
      vector<string> lineArray;
      while (getline(ss, str, ','))
        lineArray.push_back(str);
      ns3::Waypoint wpt;
      wpt.time = ns3::Time(std::to_string(time*step) + "s");
      ns3::Vector pos;
      try {
        pos.x = std::stof(lineArray[1]);
        pos.y = std::stof(lineArray[2]);
        pos.z = std::stof(lineArray[3]);
        wpt.position = pos;
        sat.waypoints.push_back(wpt);
        time++;
      } catch (...) {}
    }
    satellites[sat.name] = sat;
    NS_LOG_INFO(sat.name << " with " << sat.waypoints.size() << " points added.");
  }

  map<int, map<int, Ptr<Node>>> satArray;
  for (auto& item : satellites) {
    auto& sat = item.second;
    Ptr<Node> node = CreateObject<Node>();
    sat.node = node;
    Names::Add(sat.name, node);
  
    satNodes.Add(node);

    auto splitSatName = sin::split(sat.name, "_");
    int planeNum = std::stoi(splitSatName[1]);
    int satNum = std::stoi(splitSatName[2]);
    if (satArray.find(planeNum) == satArray.end()) {
      map<int, Ptr<Node>> satPlane;
      satArray[planeNum] = satPlane;
    }
    satArray[planeNum][satNum] = node;
    NS_LOG_INFO(sat.name << " of plane " << planeNum << ", and number " << satNum << " added to the array.");
  }

  // set up p2p links between adjacent satellites
  PointToPointHelper p2p;
  for (int plane = 0; plane < satArray.size(); plane++) {
    for (int num = 1; num < satArray[plane].size(); num++) {
      if (plane > 0)
        p2p.Install(satArray[plane][num], satArray[plane-1][num]);
      p2p.Install(satArray[plane][num], satArray[plane][num-1]);
    }
    p2p.Install(satArray[plane][0], satArray[plane][satArray[plane].size()-1]);
  }

  // read ground stations, set position
  NodeContainer stationNodes;
  map<string, sin::station> stations;
  if (stationName != "city") {
    string station_dir = dataDir + "/station/" + stationName;
    for (const auto & entry : std::experimental::filesystem::directory_iterator(station_dir)) {
      string filename = sin::split(entry.path(), "/").back();
      sin::station st;
      st.path = entry.path();
      st.name = sin::split(filename, ".").front();
      std::ifstream inFile(st.path, ios::in);
      string lineStr;
      vector<vector<string>> strArray;
      while (getline(inFile, lineStr))
      {
        stringstream ss(lineStr);
        string str;
        vector<string> lineArray;
        while (getline(ss, str, ','))
          lineArray.push_back(str);
        try {
          st.pos.x = std::stof(lineArray[0]);
          st.pos.y = std::stof(lineArray[1]);
          st.pos.z = std::stof(lineArray[2]);
          break;
        } catch (...) {}
      }
      stations[st.name] = st;
      NS_LOG_INFO(st.name << " at " << st.pos);
    }
  }
  else {
    for (auto& item : sin::CITY_LOCS) {
      sin::station st;
      st.name = item.first;
      st.pos = item.second;
      stations[st.name] = st;
      NS_LOG_INFO(st.name << " at " << st.pos);
    }
  }

  map<int, map<int, Ptr<Node>>> stationArray;
  if (stationName != "city") {
    for (auto& item : stations) {
      auto& st = item.second;
      Ptr<Node> node = CreateObject<Node>();
      st.node = node;
      Names::Add(st.name, node);
    
      // posAlloc->Add(st.pos);
      stationNodes.Add(node);

      auto splitStationName = sin::split(st.name, "_");
      int x = std::stoi(splitStationName[1]);
      int y = std::stoi(splitStationName[2]);
      if (stationArray.find(x) == stationArray.end()) {
        map<int, Ptr<Node>> stationCol;
        stationArray[x] = stationCol;
      }
      stationArray[x][y] = node;
      NS_LOG_INFO(st.name << " of col " << x << ", and row " << y << " added to the array.");
    }
  }
  else {
    for (auto& item : stations) {
      auto& st = item.second;
      Ptr<Node> node = CreateObject<Node>();
      st.node = node;
      Names::Add(st.name, node);
    
      stationNodes.Add(node);

      NS_LOG_INFO(st.name << " node created.");
    }
  }

  ndn::StackHelper ndnHelper;
  ndnHelper.UpdateFaceCreateCallback(PointToPointNetDevice::GetTypeId(), MakeCallback(&sin::p2p::SinPointToPointNetDeviceCallback));
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setCsSize(10);
  
  ndnHelper.InstallAll();

  ObjectFactory overlayFactory;
  overlayFactory.SetTypeId("ns3::sin::OverlayManager");
  for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++) {
    (*node)->AggregateObject(overlayFactory.Create<sin::OverlayManager>());
  }

  // Setting up roles
  // for producer mobility scenario, immobile consumers at one place, and multiple mobile producers at other places
  vector<pair<pair<int, int>, pair<int, int>>> stationPairs;
  {
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(7, 5)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(8, 1)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(8, 2)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(8, 3)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(8, 4)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(8, 5)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(9, 1)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(9, 2)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(9, 3)));
    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(0, 1), make_pair<int, int>(9, 4)));
  }
  vector<pair<string, string>> cityPairs;
  {
    cityPairs.push_back(make_pair<string, string>("Beijing", "London"));
    cityPairs.push_back(make_pair<string, string>("Beijing", "Moscow"));
    cityPairs.push_back(make_pair<string, string>("Beijing", "NewYork"));
    cityPairs.push_back(make_pair<string, string>("Beijing", "Rio"));
    cityPairs.push_back(make_pair<string, string>("Beijing", "Sydney"));
    cityPairs.push_back(make_pair<string, string>("Beijing", "Tokyo"));
  }
  vector<string> producerNames;
  if (stationName != "city") {
    for (auto& stPair : stationPairs) {
      std::stringstream name1;
      name1 << "Station_" << stPair.first.first << "_" << stPair.first.second;
      auto& st1 = stations[name1.str()];
      st1.isHost = true;
      st1.role = "s_consumer";
      std::stringstream name2;
      name2 << "Station_" << stPair.second.first << "_" << stPair.second.second;
      auto& st2 = stations[name2.str()];
      if (std::find(producerNames.begin(), producerNames.end(), st2.name) == producerNames.end()) {
        producerNames.push_back(st2.name);
      }
      st2.consumerStNames.push_back(st1.name);
      st2.isHost = true;
      st2.role = "m_producer";
    }
  }
  else {
    for (auto& cityPair : cityPairs) {
      auto& st1 = stations[cityPair.first];
      st1.isHost = true;
      st1.role = "s_consumer";
      auto& st2 = stations[cityPair.second];
      if (std::find(producerNames.begin(), producerNames.end(), st2.name) == producerNames.end()) {
        producerNames.push_back(st2.name);
      }
      st2.consumerStNames.push_back(st1.name);
      st2.isHost = true;
      st2.role = "m_producer";
    }
  }

  std::string rvPrefix = "/rv";

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll(rvPrefix, "/localhost/nfd/strategy/" + strategy);

  auto& satRv = satellites["Sat_18_16"];
  satRv.rvPrefix = rvPrefix; // prefix taken care of by LSN routing
  auto rvNode = satRv.node;

  // Installing applications
  if (!mock) {
    sin::GlobalRoutingHelper ndnGlobalRoutingHelper;
    ndnGlobalRoutingHelper.InstallAll();

    // RV
    ndn::AppHelper rvHelper("ns3::ndn::kite::Rv");
    rvHelper.SetAttribute("Prefix", StringValue(rvPrefix));
    rvHelper.Install(rvNode);
    ndnGlobalRoutingHelper.AddOrigin(rvPrefix, rvNode);

    for (auto name : producerNames) {
      auto& producer = stations[name];
      for (auto cName : producer.consumerStNames) {
        auto& consumer = stations[cName];
        string producerSuffix = "/" + producer.name + "/" + consumer.name;
        NS_LOG_DEBUG("Install apps for producer: " << producer.name << ", consumer: " << consumer.name << ", on prefix: " << rvPrefix + producerSuffix);

        // mobile producer
        ndn::AppHelper mobileProducerHelper("ns3::ndn::kite::Producer");
        // mobileProducerHelper.SetAttribute("PayloadSize", StringValue("1024"));
        mobileProducerHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
        mobileProducerHelper.SetAttribute("ProducerSuffix", StringValue(producerSuffix));
        mobileProducerHelper.SetAttribute("Lifetime", StringValue(std::to_string(traceLifetime)));
        mobileProducerHelper.SetAttribute("Interval", StringValue(std::to_string(refreshInterval)));
        auto mobileProducerApps = mobileProducerHelper.Install(producer.node); // first mobile node
        mobileProducerApps.Stop(Seconds(stopTime - 1));

        // consumer
        ndn::AppHelper consumerHelper("ns3::ndn::sin::Consumer");
        consumerHelper.SetPrefix(rvPrefix + producerSuffix);
        // consumerHelper.SetAttribute("Frequency", StringValue("1"));
        // consumerHelper.SetAttribute("Randomize", StringValue("uniform"));
        auto consumerApps = consumerHelper.Install(consumer.node);
        producer.consumerApps.push_back(consumerApps.Get(0));
      }
    }
  }

  sin::p2p::UpdateParams params;
  params.interval = updateInterval;
  params.curTime = 0;
  params.step = step;
  params.range = range;
  params.mock = mock;
  map<Ptr<Node>, boost::DistancesMap> distanceMap;
  sin::GlobalRoutingHelper::CalculateRoutes(&distanceMap);
  sin::p2p::UpdateLinks(params, &satellites, &stations, &distanceMap);

  Simulator::Schedule(Seconds(stopTime - 1), &sin::ShowCoverage, &stations);
  Simulator::Schedule(Seconds(stopTime - 1), &sin::p2p::ShowOverlayCount, resPrefix+"overlay-count.txt");

  ndn::sin::AppDelayTracer::InstallAll(resPrefix+"app-delays-trace.txt");
  ndn::sin::L3TrafficTracer::InstallAll(resPrefix+"l3-traffic-trace.txt");
  
  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();

  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
