/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2019 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "common.hpp"

#include "ns3/log.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-net-device.h"

NS_LOG_COMPONENT_DEFINE("ndn.common");

namespace ns3 {
namespace ndn {

std::string
time_point_to_string(std::chrono::system_clock::time_point &&tp)
{
  using namespace std;
  using namespace std::chrono;

  auto ttime_t = system_clock::to_time_t(tp);
  auto tp_sec = system_clock::from_time_t(ttime_t);
  milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

  std::tm * ttm = localtime(&ttime_t);

  char date_time_format[] = "%Y.%m.%d-%H.%M.%S";

  char time_str[] = "yyyy.mm.dd.HH-MM.SS.fff";

  strftime(time_str, strlen(time_str), date_time_format, ttm);

  string result(time_str);
  result.append(".");
  result.append(to_string(ms.count()));

  return result;
}

void
ShowProgress(int interval)
{
  std::cerr << "Progress to " << Simulator::Now ().As (Time::S) << std::endl;
  Simulator::Schedule (Seconds (interval), &ShowProgress, interval);
}

void
ShowProgress(int interval, std::chrono::system_clock::time_point lastTime, double elapsedTime)
{
  auto now = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = now - lastTime;
  elapsedTime += elapsed_seconds.count();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::cerr << "Progress to " << Simulator::Now ().As (Time::S) << " seconds, elapsed " << elapsed_seconds.count() << " seconds, " <<  elapsedTime << " seconds in total, now " << std::ctime(&now_time) << std::endl;
  Simulator::Schedule (Seconds (interval), &ShowProgress, interval, now, elapsedTime);
}

} // namespace ndn
} // namespace ns3
