/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "common.hpp"

NS_LOG_COMPONENT_DEFINE("kite.rocketfuel");

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

  string asNumber = "1239";
  cmd.AddValue("asNumber", "the rocketfuel topology to use", asNumber);

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

  NS_LOG_UNCOND("Run: " << run);
  Config::SetGlobal("RngRun", UintegerValue(run));

  Config::SetDefault("ns3::ndn::L3Protocol::DoPull", BooleanValue(doPull));

  // read the abilene topology, and set up stationary nodes
  NodeContainer topoNodes;
  topoNodes = 
    readRocketfuelEdges(string("topologies/rocketfuel_maps_cch/") + asNumber + string(".cch"),
                        1);
  NS_LOG_UNCOND("Number of edge nodes: " << topoNodes.GetN());

  Ptr<Node> consumerNode = CreateObject<Node>();
  Ptr<Node> rvNode = CreateObject<Node>();

  Ptr<UniformRandomVariable> randomAttachId = CreateObject<UniformRandomVariable>();
  randomAttachId->SetAttribute("Min", DoubleValue(0));
  randomAttachId->SetAttribute("Max", DoubleValue(topoNodes.GetN() - 1));
  int consumerAttachId = randomAttachId->GetInteger();
  int rvAttachId = randomAttachId->GetInteger();

  PointToPointHelper p2p;
  p2p.Install(consumerNode, topoNodes.Get(consumerAttachId)); // consumer <-> ?
  p2p.Install(rvNode, topoNodes.Get(rvAttachId)); // rv <-> ?

  NS_LOG_UNCOND("Consumer: " << consumerAttachId << ", RV: " << rvAttachId);

  NodeContainer apNodes;
  int apNum = 16;
  apNodes.Create(apNum);

  MobilityHelper mobility;
  Ptr<GridPositionAllocator> gridPosAlloc = CreateObject<GridPositionAllocator>();
  gridPosAlloc->SetMinX(0);
  gridPosAlloc->SetMinY(0);
  gridPosAlloc->SetDeltaX(50);
  gridPosAlloc->SetDeltaY(50);
  gridPosAlloc->SetN(4);

  mobility.SetPositionAllocator(gridPosAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  for(int i = 0; i < apNum; i++) {
    p2p.Install(apNodes.Get(i), topoNodes.Get(randomAttachId->GetInteger()));
  }

  // Create mobile nodes
  NodeContainer mobileNodes;
  mobileNodes.Create(mobileSize);

  // Setup mobility model
  Ptr<RandomRectanglePositionAllocator> randomPosAlloc =
    CreateObject<RandomRectanglePositionAllocator>();
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
  x->SetAttribute("Min", DoubleValue(-50));
  x->SetAttribute("Max", DoubleValue(250));
  randomPosAlloc->SetX(x);
  Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
  y->SetAttribute("Min", DoubleValue(-50));
  y->SetAttribute("Max", DoubleValue(250));
  randomPosAlloc->SetY(y);

  mobility.SetPositionAllocator(randomPosAlloc);
  std::stringstream ss;
  ss << "ns3::UniformRandomVariable[Min=" << speed << "|Max=" << speed << "]";

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                            RectangleValue(Rectangle(-50, 250, -50, 250)), "Distance",
                            DoubleValue(50), "Speed", StringValue(ss.str()));

  // Make mobile nodes move
  mobility.Install(mobileNodes);

  // Setup initial position of mobile node
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(100.0, 100.0, 0.0));
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(mobileNodes);

  //// Set up wifi NICs
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
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(60));
  wifiPhy.SetChannel(wifiChannel.Create());

  ////// Setup the rest of the upper mac
  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid("wifi-default");
  std::string phyMode("DsssRate1Mbps");
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  //// Setup APs.
  // NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  WifiMacHelper apWifiMacHelper;
  apWifiMacHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration",
                          BooleanValue(true), "BeaconInterval", TimeValue(Seconds(0.1)));
  wifi.Install(wifiPhy, apWifiMacHelper, apNodes);

  //// Setup STAs.
  ////// Add a non-QoS upper mac of STAs, and disable rate control
  // NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  WifiMacHelper staWifiMacHelper;
  ////// Active association of STA to AP via probing.
  staWifiMacHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true),
                           "ProbeRequestTimeout", TimeValue(Seconds(0.25)));

  wifi.Install(wifiPhy, staWifiMacHelper, mobileNodes);

  //// Install NDN stack

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  ndn::StrategyChoiceHelper::InstallAll("/", string("/localhost/nfd/strategy/") + strategy);

  std::string rvPrefix = "/rv";
  std::string producerSuffix = "/alice";

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(topoNodes);

  ndnGlobalRoutingHelper.AddOrigins(rvPrefix, topoNodes.Get(rvAttachId));

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::kite::Consumer");
  consumerHelper.SetAttribute("Prefix", StringValue(rvPrefix + producerSuffix));
  consumerHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
  consumerHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
  ApplicationContainer consumerApps = consumerHelper.Install(consumerNode); // consumer
  consumerApps.Stop(Seconds(stopTime - 1));
  consumerApps.Start(Seconds(1.0)); // delay start

  // RV
  ndn::AppHelper rvHelper("ns3::ndn::kite::Rv");
  rvHelper.SetAttribute("Prefix", StringValue(rvPrefix));
  rvHelper.Install(rvNode);

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

  ndn::AppDelayTracer::Install(consumerNode,
                               "results/kite-rocketfuel-"
                               + asNumber + "-"
                               + boost::lexical_cast<string>(run) + "-"
                               + boost::lexical_cast<string>(speed) + "-"
                               + strategy + "-"
                               + (doPull ? "pull" : "nopull") + "-"
                               + "app.txt");

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                  MakeCallback(&StaAssoc));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc",
                  MakeCallback(&StaDeAssoc));

  // ShowProgress();

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
