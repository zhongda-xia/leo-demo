/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#ifndef NDN_COMMON_HPP
#define NDN_COMMON_HPP

#include <algorithm>
#include <chrono>
#include <ctime>
#include <random>
#include <boost/algorithm/string.hpp>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"

#include "ns3/ndnSIM-module.h"

namespace ns3 {
namespace ndn {

using ::ndn::Name;
using ::ndn::PartialName;
using ::ndn::Interest;

using ::ndn::Block;
using ::ndn::operator"" _block;

std::string time_point_to_string(std::chrono::system_clock::time_point &&tp);

void
ShowProgress(int interval = 1);

void
ShowProgress(int interval, std::chrono::system_clock::time_point lastTime, double elapsedTime = 0);

} // namespace ndn
} // namespace ns3

#endif // NDN_COMMON_HPP
