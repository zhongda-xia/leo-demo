/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include <iostream>
#include <fstream>
#include <sstream>
#include <experimental/filesystem>

#include "ns3/object-factory.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-helper.h"

#include "ns3/ndnSIM/helper/ndn-network-region-table-helper.hpp"

#include "sat/common.hpp"
#include "sat/global-routing-helper.hpp"
#include "sat/overlay-manager.hpp"
#include "sat/user-link-transport.hpp"
#include "sat/app-delay-tracer.hpp"
#include "sat/l3-traffic-tracer.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.sat.scene.p2p");

namespace ns3 {

int
main(int argc, char* argv[])
{
  ns3::PacketMetadata::Enable(); // fix for visualizer

  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms")); // default delay for any ISL
  Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("10000p"));

  CommandLine cmd;

  uint32_t run = 1;
  cmd.AddValue("run", "Random seed", run);

  int stopTime = 100;
  cmd.AddValue("stop", "Simulation duration (min)", stopTime);

  string resPrefix = "default-";
  cmd.AddValue("resPrefix", "Prefix for result files", resPrefix);

  // NDN params
  string strategy = "multicast";
  cmd.AddValue("strategy", "The forwarding strategy to use", strategy);

  string consumerCbrFreq = "1.0";
  cmd.AddValue("consumerCbrFreq", "Interest sending frequency for CBR consumer", consumerCbrFreq);

  string interestLifetime = "2s";
  cmd.AddValue("interestLifetime", "Lifetime of consumer Interest, string representation", interestLifetime);

  // sat params
  int updateInterval = 1;
  cmd.AddValue("updateInterval", "The interval (min) between link change checks", updateInterval);

  uint64_t timeTillBreak = 100;
  cmd.AddValue("timeTillBreak", "The interval (ms) between the initial Interest transmission and user link breakage", timeTillBreak);

  uint64_t period = 1000;
  cmd.AddValue("period", "The period (ms) before and after handover during which consumer runs", period);

  string dataDir = ".";
  cmd.AddValue("dataDir", "Path to data files including setup configuration and traces", dataDir);

  string consumerCity = "Shanghai";
  cmd.AddValue("consumerCity", "c", consumerCity);

  string producerCity = "Delhi";
  cmd.AddValue("producerCity", "p", producerCity);

  // link service params
  bool doOverlay = false;
  cmd.AddValue("doOverlay", "enable overlay", doOverlay);
  uint64_t hopLimit = 2;
  cmd.AddValue("hopLimit", "hop limit", hopLimit);

  cmd.Parse(argc, argv);

  if (consumerCity == producerCity) {
    return -1;
  }

  Config::SetGlobal("RngRun", UintegerValue(run));

  ndn::sat::UserLinkTransport::m_doOverlay = doOverlay;
  ndn::sat::OverlayManager::m_hopLimit = hopLimit;
  ndn::sat::timeTillBreak = timeTillBreak;

  ndn::ShowProgress(updateInterval*60, std::chrono::system_clock::now());

  // read satelite and station nodes
  NodeContainer satNodes, stNodes;
  map<string, ndn::sat::satellite> satellites;
  map<string, ndn::sat::station> stations;
  map<string, vector<string>> nodesCsv = ndn::sat::readCsv(dataDir+"/nodes.csv");
  for(size_t row = 0; row < nodesCsv.begin()->second.size(); row++) {
    string name = nodesCsv["Name"].at(row);
    string type = nodesCsv["Type"].at(row);
    if (type == "Satellite") {
      ndn::sat::satellite sat;
      sat.name = name;
      Ptr<Node> node = CreateObject<Node>();
      Names::Add(name, node);
      sat.node = node;
      satellites[name] = sat;
      satNodes.Add(node);
      NS_LOG_INFO("Added satellite node " << name);
    }
    else {
      ndn::sat::station st;
      st.name = name;
      Ptr<Node> node = CreateObject<Node>();
      Names::Add(name, node);
      st.node = node;
      map<string, vector<string>> attachmentsCsv = ndn::sat::readCsv(dataDir+"/attachments_"+name+".csv");
      vector<pair<int, string>> attachments;
      for(size_t row = 0; row < attachmentsCsv.begin()->second.size(); row++) {
        attachments.push_back(make_pair(std::stoi(attachmentsCsv["Time"].at(row)),
                                                  attachmentsCsv["Satellite"].at(row)));
      }
      st.attachments = attachments;
      stations[name] = st;
      stNodes.Add(node);
      NS_LOG_INFO("Added station node " << name);
    }
  }

  // read ISL setup and set up ISLs using P2P links
  PointToPointHelper p2p;
  map<string, vector<string>> ISLs = ndn::sat::readCsv(dataDir+"/ISLs.csv");
  for (size_t row = 0; row < ISLs.begin()->second.size(); row++) {
    string first = ISLs["First"].at(row);
    string second = ISLs["Second"].at(row);
    auto sat1 = satellites[first];
    auto sat2 = satellites[second];
    p2p.Install(sat1.node, sat2.node);
    NS_LOG_INFO("Installed link between " << first << " and " << second);
  }

  ndn::StackHelper ndnHelper;
  ndnHelper.UpdateFaceCreateCallback(PointToPointNetDevice::GetTypeId(), MakeCallback(&ndn::sat::SatPointToPointNetDeviceCallback));
  ndnHelper.SetDefaultRoutes(true);
  // ndnHelper.setCsSize(10); // may need to limit CS size
  
  ndnHelper.InstallAll();

  NS_LOG_INFO("Installed NDN protocol stack");

  ObjectFactory overlayFactory;
  overlayFactory.SetTypeId("ns3::ndn::sat::OverlayManager");
  for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++) {
    (*node)->AggregateObject(overlayFactory.Create<ndn::sat::OverlayManager>());
  }

  NS_LOG_INFO("Installed Overlay Manager");

  string topPrefix = "/sat";

  // set forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");
  for (auto& item : stations) {
    ndn::StrategyChoiceHelper::Install(item.second.node, topPrefix, "/localhost/nfd/strategy/" + strategy);
  }

  // set up roles

  // for consumer mobility scenario, immobile producers at one place, and multiple mobile consumers at other places
  vector<pair<string, string>> stationPairs;
  {
    stationPairs.push_back(make_pair("city-"+consumerCity, "city-"+producerCity)); // consumer, producer
  }

  // set station roles
  vector<string> producerNames;
  for (auto& stPair : stationPairs) {
    auto& st1 = stations[stPair.first];
    st1.isHost = true;
    st1.role = "consumer";

    auto& st2 = stations[stPair.second];
    st2.isHost = true;
    st2.role = "producer";

    if (std::find(producerNames.begin(), producerNames.end(), st2.name) == producerNames.end()) {
      producerNames.push_back(st2.name);
    }
    st2.consumerStNames.push_back(stPair.first);
  }

  // read manual routes for city pairs
  map<pair<string, string>, vector<pair<int, vector<string>>>> routes;
  for (auto& stPair : stationPairs) {
    auto& st1 = stations[stPair.first];
    auto& st2 = stations[stPair.second];
    map<string, vector<string>> pairRoutesCsv = ndn::sat::readCsv(dataDir+"/routes_"+st1.name+"+"+st2.name+".csv"); // consumer, producer
    vector<pair<int, vector<string>>> pairRoutes;
    for (size_t row = 0; row < pairRoutesCsv.begin()->second.size(); row++) {
      pairRoutes.push_back(make_pair(std::stoi(pairRoutesCsv["Time"].at(row)),
                                     ndn::sat::split(pairRoutesCsv["Route"].at(row), "|")));
    }
    routes[make_pair(stPair.first, stPair.second)] = pairRoutes;
    NS_LOG_INFO("Read " << pairRoutes.size() << " routes for " << stPair.first << " " << stPair.second);
  }

  // read producer routes
  map<string, vector<pair<int, map<string, vector<pair<string, string>>>>>> producerRoutes;
  for (const auto& producerName : producerNames) {
    auto& station = stations[producerName];
    BOOST_ASSERT(station.isHost);
    BOOST_ASSERT(station.role == "producer");

    map<string, vector<string>> routesCsv = ndn::sat::readCsv(dataDir+"/routes_"+station.name+".csv");
    vector<pair<int, map<string, vector<pair<string, string>>>>> pRoutes;
    map<int, int> epochFlags;
    for (size_t row = 0; row < routesCsv.begin()->second.size(); row++) {
      int epoch = std::stoi(routesCsv["Time"].at(row));
      string op = routesCsv["Op"].at(row);
      string from = routesCsv["From"].at(row);
      string to = routesCsv["To"].at(row);
      auto epochIndex = epochFlags.find(epoch);
      if (epochIndex == epochFlags.end()) {
        pRoutes.push_back(make_pair(epoch, map<string, vector<pair<string, string>>>()));
        pRoutes.back().second["add"] = vector<pair<string, string>>();
        pRoutes.back().second["remove"] = vector<pair<string, string>>();
        epochFlags[epoch] = pRoutes.size()-1;
      }
      pRoutes[epochFlags[epoch]].second[op].push_back(make_pair(from, to));
    }
    producerRoutes[station.name] = pRoutes;
    NS_LOG_INFO("Read routes for " << station.name);
  }

  // Installing applications
  ndn::sat::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  for (auto name : producerNames) {
    auto& producer = stations[name];
    string producerPrefix = topPrefix + "/" + producer.name;

    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix(producerPrefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
    auto producerApps = producerHelper.Install(producer.node);
    NS_LOG_INFO("Installed apps for producer: " << producer.name << ", on prefix: " << producerPrefix);

    // ndnGlobalRoutingHelper.AddOrigin(producerPrefix, producer.node);

    producer.prefixes.push_back(producerPrefix);

    for (auto cName : producer.consumerStNames) {
      auto& consumer = stations[cName];
      string prefix = producerPrefix + "/" + consumer.name;

      // ndn::AppHelper consumerHelper("ns3::ndn::sat::Consumer");
      // consumerHelper.SetPrefix(prefix);
      // // consumerHelper.SetAttribute("Frequency", StringValue("1"));
      // // consumerHelper.SetAttribute("Randomize", StringValue("uniform"));
      // auto consumerApps = consumerHelper.Install(consumer.node);
      // producer.consumerApps.push_back(consumerApps.Get(0));

      ndn::AppHelper consumerHelper("ns3::ndn::sat::ConsumerCbr");
      consumerHelper.SetPrefix(prefix);
      consumerHelper.SetAttribute("Frequency", StringValue(consumerCbrFreq));
      // consumerHelper.SetAttribute("Randomize", StringValue("uniform"));
      auto consumerApps = consumerHelper.Install(consumer.node);
      producer.consumerApps.push_back(consumerApps.Get(0));

      ndn::sat::AppDelayTracer::Install(consumer.node, resPrefix+"app-delays-trace.txt");

      NS_LOG_INFO("Installed apps for producer: " << producer.name << ", consumer: " << consumer.name << ", on prefix: " << prefix);
    }
  }

  if (strategy == "hint") {
    string satTopPrefix = "/nodes/sats";
    for (auto& item : stations) {
      auto& station = item.second;
      if (station.role != "consumer") {
        continue;
      }
      for (auto& att : station.attachments) {
        auto& satellite = satellites[att.second];
        satellite.satPrefix = satTopPrefix+"/"+satellite.name;
        ndnGlobalRoutingHelper.AddOrigin(satellite.satPrefix, satellite.node);
        ndn::NetworkRegionTableHelper::AddRegionName(satellite.node, satellite.satPrefix);
      }
    }
    ndnGlobalRoutingHelper.CalculateRoutes();
  }

  ndn::sat::UpdateParams params;
  params.interval = updateInterval;
  params.curTime = 0;
  params.period = period;
  ndn::sat::Update(params, &satellites, &stations, &routes, &producerRoutes);

  Simulator::Schedule(Seconds(stopTime*60-1), &ndn::sat::ShowOverlayCount, resPrefix+"overlay-count.txt");

  ndn::sat::L3TrafficTracer::InstallAll(resPrefix+"l3-traffic-trace.txt");

  Simulator::Stop(Seconds(stopTime*60));
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
