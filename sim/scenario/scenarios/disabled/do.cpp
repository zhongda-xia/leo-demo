/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "common.hpp"

NS_LOG_COMPONENT_DEFINE("kite.do");

namespace ns3 {

NodeContainer
setTopology(const string& type, const string& topoParam)
{
  NodeContainer topoNodes;
  if (type == "rocketfuel") {
    topoNodes = readRocketfuel("topologies/rocketfuel_maps_cch/" + topoParam + ".cch", 1);
  }
  else if (type == "cooked-rocketfuel") {
    topoNodes = readCookedRocketfuel("topologies/cooked_rocketfuel/" + topoParam + ".txt");
  }
  NS_LOG_UNCOND("(" << type << "-" << topoParam << ") Number of edge nodes: " << topoNodes.GetN());
  return topoNodes;
}

NodeContainer
setAp(int x, int y, int gridSize, int xShift = 0, int yShift = 0)
{
  NodeContainer apNodes;
  int apNum = x * y;
  apNodes.Create(apNum);

  MobilityHelper mobility;
  Ptr<GridPositionAllocator> gridPosAlloc = CreateObject<GridPositionAllocator>();
  gridPosAlloc->SetMinX(xShift);
  gridPosAlloc->SetMinY(yShift);
  gridPosAlloc->SetDeltaX(gridSize);
  gridPosAlloc->SetDeltaY(gridSize);
  gridPosAlloc->SetN(x);

  mobility.SetPositionAllocator(gridPosAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  return apNodes;
}

NodeContainer
setMobileNodes(int mobileSize, float speed, int gridSize, int apX, int apY, int xShift = 0, int yShift = 0)
{
  apX -= 1;
  apY -= 1;
  // Create mobile nodes
  NodeContainer mobileNodes;
  mobileNodes.Create(mobileSize);

  // Setup bounds
  int xMin = (-0.5) * gridSize + xShift;
  int xMax = (apX + 0.5) * gridSize + xShift;
  int yMin = (-0.5) * gridSize + yShift;
  int yMax = (apY + 0.5) * gridSize + yShift;

  // Setup initial position of mobile node
  MobilityHelper mobility;
  Ptr<RandomRectanglePositionAllocator> randomPosAlloc =
    CreateObject<RandomRectanglePositionAllocator>();
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
  x->SetAttribute("Min", DoubleValue(xMin));
  x->SetAttribute("Max", DoubleValue(xMax));
  x->SetAttribute("Stream", IntegerValue(0));
  randomPosAlloc->SetX(x);
  Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
  y->SetAttribute("Min", DoubleValue(yMin));
  y->SetAttribute("Max", DoubleValue(yMax));
  y->SetAttribute("Stream", IntegerValue(0));
  randomPosAlloc->SetY(y);

  mobility.SetPositionAllocator(randomPosAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  // Make mobile nodes move
  std::stringstream ss;
  ss << "ns3::UniformRandomVariable[Min=" << speed << "|Max=" << speed << "|Stream=0" << "]";

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                            RectangleValue(Rectangle(xMin, xMax, yMin, yMax)), "Distance",
                            DoubleValue(gridSize * 0.7), "Speed", StringValue(ss.str()));
  mobility.Install(mobileNodes);

  // Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  // posAlloc->Add(Vector(0.0, 0.0, 0.0));
  // mobility.SetPositionAllocator(posAlloc);
  // mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(mobileNodes);

  return mobileNodes;
}

void
installWifi5(const NodeContainer& apNodes, const NodeContainer& mobileNodes, int gridSize)
{
  //// Set up wifi NICs
  ////// The below set of helpers will help us to put together the wifi NICs we want

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

  ////// This is one parameter that matters when using FixedRssLossModel
  ////// set it to zero; otherwise, gain will be added
  // wifiPhy.Set ("RxGain", DoubleValue (0) );

  ////// ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  // wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;

  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

  // wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  ////// The below FixedRssLossModel will cause the rss to be fixed regardless
  ////// of the distance between the two stations, and the transmit power
  // wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue(rss));

  ////// the following has an absolute cutoff at distance > range (range == radius)
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(gridSize * 0.707));
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set ("ShortGuardEnabled", BooleanValue (1));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

  ////// Setup the rest of the upper mac
  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid("wifi-default");
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");
  // std::string phyMode("DsssRate1Mbps");
  // wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode),
  //                              "ControlMode", StringValue(phyMode));
  wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

  WifiMacHelper wifiMacHelper;

  // for 802.11n 2-LEVEL data frame aggregation:
  // int nMpdu = 7;
  // int nMsdu = 4;
  // int payloadSize = BITS_TO_BYTES(1024);
  // int interestSize = 64;

  // for station:
	// -------------frame aggregation(aggregate data packets at producer)----------
	// wifiMacHelper.SetMpduAggregatorForAc (AC_BE, "ns3::MpduStandardAggregator"
	//                             ,"MaxAmpduSize", UintegerValue (nMpdu * (nMsdu * (payloadSize + 100)))
	//                             );
  // wifiMacHelper.SetMsduAggregatorForAc (AC_BE, "ns3::MsduStandardAggregator"
	//                             ,"MaxAmsduSize", UintegerValue (nMsdu * (payloadSize + 100))
	//                             );

  /* NOTE: the block ack mechanism in current ns3 release is not robust,
    * if runtime error occurs in the simulation, try to remove the following line of code, 
    * without block ack we can still achieve a maximum throughput of 70Mbits/s in 802.11n with NDN.
    */
 	// wifiMacHelper.SetBlockAckThresholdForAc (AC_BE, 2);
	// wifiMacHelper.SetBlockAckInactivityTimeoutForAc (AC_BE, 400);

  //// Setup APs.
  wifiMacHelper.SetType("ns3::ApWifiMac"
                        , "Ssid", SsidValue(ssid)
                        , "EnableBeaconJitter", BooleanValue (true)
                        // , "BeaconGeneration", BooleanValue(true)
                        // , "BeaconInterval", TimeValue(Seconds(0.1))
                        );
  wifi.Install(wifiPhy, wifiMacHelper, apNodes);

  //// Setup STAs.
  ////// Active association of STA to AP via probing.

  wifiMacHelper.SetType ("ns3::StaWifiMac"
                         ,"Ssid", SsidValue (ssid)
                         ,"ActiveProbing", BooleanValue (true)
                         ,"MaxMissedBeacons", UintegerValue (3)
                         ,"AssocRequestTimeout", TimeValue (Seconds (0.05))
                         ,"ProbeRequestTimeout", TimeValue (Seconds (0.05))
			                   );

  wifi.Install(wifiPhy, wifiMacHelper, mobileNodes);
}

void
installWifi(const NodeContainer& apNodes, const NodeContainer& mobileNodes, int gridSize, double rangeFactor)
{
  //// Set up wifi NICs
  ////// The below set of helpers will help us to put together the wifi NICs we want

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
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(gridSize * rangeFactor));
  wifiPhy.SetChannel(wifiChannel.Create());

  ////// Setup the rest of the upper mac
  WifiHelper wifi;

  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  std::string phyMode("DsssRate1Mbps");
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager"
                               , "DataMode", StringValue(phyMode)
                               , "ControlMode", StringValue(phyMode)
                               );

  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid("wifi-default");

  //// Setup APs.
  WifiMacHelper apWifiMacHelper;
  apWifiMacHelper.SetType("ns3::ApWifiMac"
                          , "Ssid", SsidValue(ssid)
                          , "BeaconGeneration", BooleanValue(true)
                          , "BeaconInterval", TimeValue(Seconds(0.1))
                          );
  wifi.Install(wifiPhy, apWifiMacHelper, apNodes);

  //// Setup STAs.
  ////// Add a non-QoS upper mac of STAs, and disable rate control
  WifiMacHelper staWifiMacHelper;
  ////// Active association of STA to AP via probing.
  staWifiMacHelper.SetType("ns3::StaWifiMac"
                           , "Ssid", SsidValue(ssid)
                           , "ActiveProbing", BooleanValue(true)
                           , "ProbeRequestTimeout", TimeValue(Seconds(0.25))
                           );

  wifi.Install(wifiPhy, staWifiMacHelper, mobileNodes);
}

void
connectCallbacks(const string& solution)
{
  if (solution == "kite") {
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                    MakeCallback(&StaAssoc));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc",
                    MakeCallback(&StaDeAssoc));
  }
  else if (solution == "mapme") {
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                    MakeCallback(&StaAssoc_Mapme));
  }
  else if (solution == "mapping") {
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                    MakeCallback(&StaAssoc_Mapping));
  }
}

void
installApps(const string& solution, const string& rvPrefix, const string& producerSuffix
            , const float consumerCbrFreq, const string& interestLifetime, const int stopTime
            , const int traceLifetime, const int refreshInterval
            , const Ptr<Node>& consumerNode, const Ptr<Node>& producerNode, const Ptr<Node>& rvNode)
{
  // Installing applications

  if (solution == "kite") {
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::kite::Consumer");
    consumerHelper.SetAttribute("Prefix", StringValue(rvPrefix + producerSuffix));
    consumerHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
    consumerHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
    ApplicationContainer consumerApps = consumerHelper.Install(consumerNode); // consumer
    consumerApps.Stop(Seconds(stopTime - 1));
    consumerApps.Start(Seconds(1.0)); // delay start

    // Mobile producer
    ndn::AppHelper mobileProducerHelper("ns3::ndn::kite::Producer");
    mobileProducerHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
    mobileProducerHelper.SetAttribute("ProducerSuffix", StringValue(producerSuffix));
    mobileProducerHelper.SetAttribute("Lifetime", StringValue(std::to_string(traceLifetime)));
    mobileProducerHelper.SetAttribute("Interval", StringValue(std::to_string(refreshInterval)));
    ApplicationContainer mobileProducerApps =
      mobileProducerHelper.Install(producerNode); // should be first mobile node
    mobileProducerApps.Stop(Seconds(stopTime - 1));

    // RV
    ndn::AppHelper rvHelper("ns3::ndn::kite::Rv");
    rvHelper.SetAttribute("Prefix", StringValue(rvPrefix));
    rvHelper.Install(rvNode);
  }
  else if (solution == "mapme") {
    string producerPrefix = rvPrefix + producerSuffix;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::kite::Consumer");
    consumerHelper.SetAttribute("Prefix", StringValue(producerPrefix));
    consumerHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
    consumerHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
    ApplicationContainer consumerApps = consumerHelper.Install(consumerNode); // consumer
    consumerApps.Stop(Seconds(stopTime - 1));
    consumerApps.Start(Seconds(1.0)); // delay start

    // Mobile producer
    ndn::AppHelper mobileProducerHelper("ns3::ndn::mapme::Producer");
    mobileProducerHelper.SetAttribute("Prefix", StringValue(producerPrefix));
    ApplicationContainer mobileProducerApps =
      mobileProducerHelper.Install(producerNode); // first mobile node
    mobileProducerApps.Stop(Seconds(stopTime - 1));
  }
  else if (solution == "mapping") {
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::mapping::Consumer");
    consumerHelper.SetAttribute("Prefix", StringValue("/"));
    consumerHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
    consumerHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
    consumerHelper.SetAttribute("ServerPrefix", StringValue(rvPrefix));
    ApplicationContainer consumerApps = consumerHelper.Install(consumerNode); // consumer
    consumerApps.Stop(Seconds(stopTime - 1));
    consumerApps.Start(Seconds(1.0)); // delay start

    // Mobile producer
    ndn::AppHelper mobileProducerHelper("ns3::ndn::mapping::Producer");
    mobileProducerHelper.SetAttribute("Prefix", StringValue("/ap")); // "/ap" is AP prefix
    mobileProducerHelper.SetAttribute("ServerPrefix", StringValue(rvPrefix));
    ApplicationContainer mobileProducerApps =
      mobileProducerHelper.Install(producerNode); // should be first mobile node
    mobileProducerApps.Stop(Seconds(stopTime - 1));

    // Mapping server
    ndn::AppHelper serverHelper("ns3::ndn::mapping::Server");
    serverHelper.SetAttribute("Prefix", StringValue(rvPrefix));
    serverHelper.Install(rvNode);
  }
  else if (solution == "test") {
    // consumer mobility scenario
    string producerPrefix = rvPrefix + producerSuffix;
    // Mobile consumer (on producerNode)
    ndn::AppHelper consumerHelper("ns3::ndn::kite::Consumer");
    consumerHelper.SetAttribute("Prefix", StringValue(producerPrefix));
    consumerHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
    consumerHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
    ApplicationContainer consumerApps = consumerHelper.Install(producerNode); // first mobile node
    consumerApps.Stop(Seconds(stopTime - 1));
    consumerApps.Start(Seconds(1.0)); // delay start

    // Producer (on consumerNode)
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix(producerPrefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
    ApplicationContainer producerApps =
      producerHelper.Install(consumerNode);
    producerApps.Stop(Seconds(stopTime - 1));
  }
}

int
main(int argc, char* argv[])
{
  ns3::PacketMetadata::Enable(); // for visualizer error

  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("10000p"));

  CommandLine cmd;

  string wifiMode = "default";
  cmd.AddValue("wifiMode", "WIFI mode", wifiMode);

  double rangeFactor = 0.5;
  cmd.AddValue("rangeFactor", "range factor", rangeFactor);

  string resultPrefix = "result";
  cmd.AddValue("resultPrefix", "the prefix of the name of output files", resultPrefix);

  int run = 1;
  cmd.AddValue("run", "Run #", run);

  int stopTime = 100;
  cmd.AddValue("stop", "stop time", stopTime);

  string solution = "kite";
  cmd.AddValue("solution", "the solution to evaluate", solution);

  int mobileSize = 1;
  cmd.AddValue("size", "# mobile", mobileSize);
  float speed = 15;
  cmd.AddValue("speed", "mobile speed m/s", speed);

  int apGridSize = 50;
  cmd.AddValue("apGridSize", "AP grid size m", apGridSize);

  string topology = "rocketfuel";
  cmd.AddValue("topology", "the topology type", topology);
  string topoParam = "6461";
  cmd.AddValue("topoParam", "type-specific information for topology", topoParam);

  string strategy = "multicast";
  cmd.AddValue("strategy", "the forwarding strategy to use", strategy);

  // KITE-specific parameters
  int traceLifetime = 2000;
  cmd.AddValue("traceLifetime", "trace lifetime (ms)", traceLifetime);
  int refreshInterval = 2000;
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


  NodeContainer topoNodes = setTopology(topology, topoParam);

  int apX = 4, apY = 4, xShift = -200, yShift = 0;
  NodeContainer apNodes = setAp(apX, apY, apGridSize, xShift, yShift);

  NodeContainer mobileNodes = setMobileNodes(mobileSize, speed, apGridSize, apX, apY, xShift, yShift);

  if (wifiMode == "5"){
    installWifi5(apNodes, mobileNodes, apGridSize);
  }
  else {
    installWifi(apNodes, mobileNodes, apGridSize, rangeFactor);
  }

  //// Attach roles

  // Ptr<UniformRandomVariable> randomAttachId = CreateObject<UniformRandomVariable>();
  // randomAttachId->SetAttribute("Min", DoubleValue(0));
  // randomAttachId->SetAttribute("Max", DoubleValue(topoNodes.GetN() - 1));
  // randomAttachId->SetAttribute("Stream", IntegerValue(0));

  AttachId attacher(topoNodes.GetN(), run);

  Ptr<Node> consumerNode = CreateObject<Node>();
  Ptr<Node> rvNode = CreateObject<Node>();
  // int consumerAttachId = randomAttachId->GetInteger();
  // int rvAttachId = randomAttachId->GetInteger();
  int consumerAttachId = attacher.get();
  int rvAttachId = attacher.get();
  PointToPointHelper p2p;
  p2p.Install(consumerNode, topoNodes.Get(consumerAttachId)); // consumer <-> ?
  p2p.Install(rvNode, topoNodes.Get(rvAttachId)); // rv <-> ?

  NS_LOG_UNCOND("Consumer: " << consumerAttachId << ", RV: " << rvAttachId);

  int* apAttachIds = new int[apX * apY];
  if (topology == "rocketfuel" || topology == "cooked-rocketfuel") {
    for(int i = 0; i < apX * apY; i++) {
      // int attachId = randomAttachId->GetInteger();
      int attachId = attacher.get();
      // p2p.Install(apNodes.Get(i), topoNodes.Get(attachId));
      apAttachIds[i] = attachId;
    }
  }

  //// Install NDN stack

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  if (solution == "kite") {
    ndn::StrategyChoiceHelper::InstallAll("/", string("/localhost/nfd/strategy/") + strategy);
  }
  else if (solution == "mapme") {
    ndn::StrategyChoiceHelper::InstallAll("/", string("/localhost/nfd/strategy/mapme"));
  }
  else if (solution == "mapping") {
    ndn::StrategyChoiceHelper::InstallAll("/", string("/localhost/nfd/strategy/multicast"));
  }

  std::string rvPrefix = "/rv"; // also mapping server's prefix
  std::string producerSuffix = "/alice";

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(topoNodes);
  // ndnGlobalRoutingHelper.Install(apNodes);

  if (solution == "kite") {
    ndnGlobalRoutingHelper.AddOrigin(rvPrefix, topoNodes.Get(rvAttachId));
  }
  else if (solution == "mapme") {
    // make sure that mapme updates can go out
    auto mobileNode = mobileNodes.Get(0);
    BOOST_ASSERT(mobileNode->GetNDevices() == 1);
    auto mobileWifiFace = mobileNode->GetObject<ndn::L3Protocol>()->getFaceByNetDevice(mobileNode->GetDevice(0));
    Name mapmePrefix(rvPrefix + producerSuffix);
    ndn::FibHelper::AddRoute(mobileNode, Name(mapmePrefix).append(MAPME_KEYWORD), mobileWifiFace, 1);

    // announce producer prefix somewhere (not MP's initial position)
    Ptr<Node> strawmanNode = CreateObject<Node>();
    int strawmanAttachId = apAttachIds[0];
    auto topoNode = topoNodes.Get(strawmanAttachId);
    auto devices = p2p.Install(strawmanNode, topoNode);
    ndnHelper.Install(strawmanNode);
    ndnHelper.Update(topoNode);
    ndnGlobalRoutingHelper.Install(strawmanNode);
    // add missing edge on topoNode
    topoNode->GetObject<ndn::GlobalRouter>()->AddIncidency(topoNode->GetObject<ndn::L3Protocol>()->getFaceByNetDevice(devices.Get(0))
                                                           , strawmanNode->GetObject<ndn::GlobalRouter>()
                                                           );
    ndnGlobalRoutingHelper.AddOrigin(mapmePrefix.toUri(), strawmanNode);
    ndn::FibHelper::AddRoute(topoNode, mapmePrefix, strawmanNode, 1);
  }
  else if (solution == "mapping") {
    ndnGlobalRoutingHelper.AddOrigin(rvPrefix, topoNodes.Get(rvAttachId));
    for (int i = 0; i < apX * apY; i++) {
      auto topoNode = topoNodes.Get(apAttachIds[i]);
      std::string apPrefix = "/ap/" + std::to_string(topoNode->GetId());
      ndnGlobalRoutingHelper.AddOrigin(apPrefix, topoNode);
      std::cerr << "Setting name for " << i << " " << topoNode->GetId() << " " << apPrefix << std::endl;
      Names::Add(std::to_string(topoNode->GetId()), apNodes.Get(i));
      // add route to AP for "/ap"
      // ndn::FibHelper::AddRoute(topoNode, Name("/ap"), apNodes.Get(i), 1);
    }
  }
  else if (solution == "test") {
    ndnGlobalRoutingHelper.AddOrigin(rvPrefix + producerSuffix, topoNodes.Get(consumerAttachId));
  }

  ndn::GlobalRoutingHelper::CalculateRoutes();

  // postpone AP connection due to routing issues (unneeded routing entries on APs)
  for (int i = 0; i < apX * apY; i++) {
    auto apNode = apNodes.Get(i);
    auto topoNode = topoNodes.Get(apAttachIds[i]);
    // create p2p link and add face
    p2p.Install(apNode, topoNode);
    ndnHelper.Update(apNode);
    ndnHelper.Update(topoNode);
    // add default route on AP
    // ndn::FibHelper::AddRoute(apNode, Name("/"), topoNode, 1);
    // auto wifiFace = topoNode->GetObject<ndn::L3Protocol>()->getFaceByNetDevice(apNode->GetDevice(0));
    // ndn::FibHelper::AddRoute(apNode, Name("/"), wifiFace, 1);
    // add route for producer prefix on APs (or MapME won't work)
    if (solution == "mapme") {
      Name mapmePrefix(rvPrefix + producerSuffix);
      ndn::FibHelper::AddRoute(apNode, mapmePrefix, topoNode, 1);
    }
    // add route to AP for "/ap"
    else if (solution == "mapping") {
      ndn::FibHelper::AddRoute(topoNode, Name("/ap"), apNode, 1);
    }
  }

  installApps(solution, rvPrefix, producerSuffix
              , consumerCbrFreq, interestLifetime, stopTime
              , traceLifetime, refreshInterval
              , consumerNode, mobileNodes.Get(0), rvNode);

  if (solution == "test") {
    ndn::AppDelayTracer::Install(mobileNodes.Get(0),
                                 "results/"
                                 + resultPrefix + "-"
                                 + "app.txt");
  }
  else {
    ndn::AppDelayTracer::Install(consumerNode,
                                 "results/"
                                 + resultPrefix + "-"
                                 + "app.txt");
  }

  connectCallbacks(solution);

  ShowProgress(5);

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
