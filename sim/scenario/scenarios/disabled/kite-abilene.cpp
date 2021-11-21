/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "common.hpp"

NS_LOG_COMPONENT_DEFINE("kite.abilene");

namespace ns3 {

int
main(int argc, char* argv[])
{
  ns3::PacketMetadata::Enable();

  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("10000p"));

  CommandLine cmd;

  uint32_t run = 1;
  cmd.AddValue("run", "Run", run);

  int mobileSize = 1;
  cmd.AddValue("size", "# mobile", mobileSize);
  float speed = 30;
  cmd.AddValue("speed", "mobile speed m/s", speed);
  int stopTime = 100;
  cmd.AddValue("stop", "stop time", stopTime);

  string strategy = "multicast";
  cmd.AddValue("strategy", "the forwarding strategy to use", strategy);

  uint32_t traceLifeTime = 2000;
  cmd.AddValue("traceLifeTime", "trace lifetime (ms)", traceLifeTime);
  uint32_t refreshInterval = 2000;
  cmd.AddValue("refreshInterval", "refresh interval (ms)", refreshInterval);

  float consumerCbrFreq = 1.0;
  cmd.AddValue("consumerCbrFreq", "", consumerCbrFreq);

  bool doPull = false;
  cmd.AddValue("doPull", "enable pulling", doPull);

  string interestLifetime = "2s";
  cmd.AddValue("interestLifetime", "lifetime of consumer Interest", interestLifetime);

  cmd.Parse(argc, argv);

  Config::SetGlobal("RngRun", UintegerValue(run));

  Config::SetDefault("ns3::ndn::L3Protocol::DoPull", BooleanValue(doPull));

  // read the abilene topology, and set up stationary nodes
  NodeContainer nodes;
  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("topologies/topo-abilene.txt");
  nodes = topologyReader.Read();

  nodes.Create(2); // consumer and RV

  PointToPointHelper p2p;
  p2p.Install(nodes.Get(11), nodes.Get(0)); // consumer <-> ?
  p2p.Install(nodes.Get(12), nodes.Get(10)); // rv <-> 1

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 0.0, 0.0));
  posAlloc->Add(Vector(0.0, 100.0, 0.0));
  posAlloc->Add(Vector(100.0, -100.0, 0.0));
  posAlloc->Add(Vector(100.0, 0, 0.0));
  posAlloc->Add(Vector(100.0, 100.0, 0.0));
  posAlloc->Add(Vector(100.0, 200.0, 0.0));
  posAlloc->Add(Vector(100.0, -200.0, 0.0));
  posAlloc->Add(Vector(200.0, 0.0, 0.0));
  posAlloc->Add(Vector(200.0, 100.0, 0.0));
  posAlloc->Add(Vector(200.0, -100.0, 0.0));
  posAlloc->Add(Vector(200.0, -200.0, 0.0));

  posAlloc->Add(Vector(10.0, 10.0, 0.0));
  posAlloc->Add(Vector(90.0, 90.0, 0.0));

  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  std::string phyMode("DsssRate1Mbps");
  ////// The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

  ////// This is one parameter that matters when using FixedRssLossModel
  ////// set it to zero; otherwise, gain will be added
  // wifiPhy.Set ("RxGain", DoubleValue (0) );

  ////// ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;

  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  // wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  ////// The below FixedRssLossModel will cause the rss to be fixed regardless
  ////// of the distance between the two stations, and the transmit power
  // wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue(rss));

  ////// the following has an absolute cutoff at distance > range (range == radius)
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100));
  wifiPhy.SetChannel(wifiChannel.Create());

  ////// Setup the rest of the upper mac
  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid("wifi-default");
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  ////// Add a non-QoS upper mac of STAs, and disable rate control
  // NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  WifiMacHelper wifiMac;
  ////// Active associsation of STA to AP via probing.
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true),
                  "ProbeRequestTimeout", TimeValue(Seconds(0.25)));

  // Create mobile nodes
  NodeContainer mobileNodes;
  mobileNodes.Create(mobileSize);

  wifi.Install(wifiPhy, wifiMac, mobileNodes);

  // Setup mobility model
  Ptr<RandomRectanglePositionAllocator> randomPosAlloc =
    CreateObject<RandomRectanglePositionAllocator>();
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
  x->SetAttribute("Min", DoubleValue(100));
  x->SetAttribute("Max", DoubleValue(200));
  randomPosAlloc->SetX(x);
  Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
  y->SetAttribute("Min", DoubleValue(-200));
  y->SetAttribute("Max", DoubleValue(200));
  randomPosAlloc->SetY(y);

  mobility.SetPositionAllocator(randomPosAlloc);
  std::stringstream ss;
  ss << "ns3::UniformRandomVariable[Min=" << speed << "|Max=" << speed << "]";

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                            RectangleValue(Rectangle(100, 200, -200, 200)), "Distance",
                            DoubleValue(200), "Speed", StringValue(ss.str()));

  // Make mobile nodes move
  mobility.Install(mobileNodes);

  // Setup initial position of mobile node
  posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(100.0, 100.0, 0.0));
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(mobileNodes);

  ////// Setup AP.
  // NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  WifiMacHelper wifiMacHelper;
  wifiMacHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration",
                        BooleanValue(true), "BeaconInterval", TimeValue(Seconds(0.1)));
  for (int i = 0; i < 11; i++) {
    wifi.Install(wifiPhy, wifiMacHelper, nodes.Get(i));
  }

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  ndn::StrategyChoiceHelper::InstallAll("/", string("/localhost/nfd/strategy/") + strategy);

  std::string rvPrefix = "/rv";
  std::string producerSuffix = "/alice";

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);

  ndnGlobalRoutingHelper.AddOrigins(rvPrefix, nodes.Get(12));

  // Installing applications

  // Consumer
  int consumerNodeId = 11;
  ndn::AppHelper consumerHelper("ns3::ndn::kite::Consumer");
  consumerHelper.SetAttribute("Prefix", StringValue(rvPrefix + producerSuffix));
  consumerHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
  consumerHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
  ApplicationContainer consumerApps = consumerHelper.Install(nodes.Get(consumerNodeId)); // consumer
  consumerApps.Stop(Seconds(stopTime - 1));
  consumerApps.Start(Seconds(1.0)); // delay start

  // RV
  ndn::AppHelper rvHelper("ns3::ndn::kite::Rv");
  rvHelper.SetAttribute("Prefix", StringValue(rvPrefix));
  rvHelper.Install(nodes.Get(12));

  // Mobile producer
  ndn::AppHelper mobileProducerHelper("ns3::ndn::kite::Producer");
  mobileProducerHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
  mobileProducerHelper.SetAttribute("ProducerSuffix", StringValue(producerSuffix));
  mobileProducerHelper.SetAttribute("Lifetime", StringValue(std::to_string(traceLifeTime)));
  mobileProducerHelper.SetAttribute("Interval", StringValue(std::to_string(refreshInterval)));
  ApplicationContainer mobileProducerApps =
    mobileProducerHelper.Install(mobileNodes.Get(0)); // first mobile node
  mobileProducerApps.Stop(Seconds(stopTime - 1));

  ndn::GlobalRoutingHelper::CalculateRoutes();

  ndn::AppDelayTracer::Install(nodes.Get(consumerNodeId),
                               "results/kite-abilene-"
                               + boost::lexical_cast<string>(run) + "-"
                               + boost::lexical_cast<string>(speed) + "-"
                               + strategy + "-"
                               + (doPull ? "pull" : "nopull") + "-"
                               + "app.txt");

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                  MakeCallback(&StaAssoc));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc",
                  MakeCallback(&StaDeAssoc));

  for (int i = 0; i < 11; i++) {
    NS_LOG_INFO("Node: " << i);
    getNodeInfo(nodes.Get(i));
  }

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