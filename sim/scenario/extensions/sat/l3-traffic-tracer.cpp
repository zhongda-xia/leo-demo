/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2016  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "l3-traffic-tracer.hpp"

#include "handover-manager.hpp"

#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/node-list.h"

#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/NFD/daemon/table/pit-entry.hpp"

#include <fstream>
#include <boost/lexical_cast.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.sat.L3TrafficTracer");

namespace ns3 {
namespace ndn {
namespace sat {

using std::make_pair;

const Name s_commonPrefix("/sin");

static std::list<std::tuple<shared_ptr<std::ostream>, std::list<Ptr<L3TrafficTracer>>>>
  g_tracers;

void
L3TrafficTracer::Destroy()
{
  g_tracers.clear();
}

void
L3TrafficTracer::InstallAll(const std::string& file)
{
  std::list<Ptr<L3TrafficTracer>> tracers;
  shared_ptr<std::ostream> outputStream;
  if (file != "-") {
    shared_ptr<std::ofstream> os(new std::ofstream());
    os->open(file.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
      return;
    }

    outputStream = os;
  }
  else {
    outputStream = shared_ptr<std::ostream>(&std::cout, std::bind([]{}));
  }

  for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++) {
    Ptr<L3TrafficTracer> trace = Install(*node, outputStream);
    tracers.push_back(trace);
  }

  if (tracers.size() > 0) {
    // *m_l3RateTrace << "# "; // not necessary for R's read.table
    tracers.front()->PrintHeader(*outputStream);
    *outputStream << "\n";
  }

  g_tracers.push_back(std::make_tuple(outputStream, tracers));
}

void
L3TrafficTracer::Install(const NodeContainer& nodes, const std::string& file)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<L3TrafficTracer>> tracers;
  shared_ptr<std::ostream> outputStream;
  if (file != "-") {
    shared_ptr<std::ofstream> os(new std::ofstream());
    os->open(file.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
      return;
    }

    outputStream = os;
  }
  else {
    outputStream = shared_ptr<std::ostream>(&std::cout, std::bind([]{}));
  }

  for (NodeContainer::Iterator node = nodes.Begin(); node != nodes.End(); node++) {
    Ptr<L3TrafficTracer> trace = Install(*node, outputStream);
    tracers.push_back(trace);
  }

  if (tracers.size() > 0) {
    // *m_l3RateTrace << "# "; // not necessary for R's read.table
    tracers.front()->PrintHeader(*outputStream);
    *outputStream << "\n";
  }

  g_tracers.push_back(std::make_tuple(outputStream, tracers));
}

void
L3TrafficTracer::Install(Ptr<Node> node, const std::string& file)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<L3TrafficTracer>> tracers;
  shared_ptr<std::ostream> outputStream;
  if (file != "-") {
    shared_ptr<std::ofstream> os(new std::ofstream());
    os->open(file.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
      return;
    }

    outputStream = os;
  }
  else {
    outputStream = shared_ptr<std::ostream>(&std::cout, std::bind([]{}));
  }

  Ptr<L3TrafficTracer> trace = Install(node, outputStream);
  tracers.push_back(trace);

  if (tracers.size() > 0) {
    // *m_l3RateTrace << "# "; // not necessary for R's read.table
    tracers.front()->PrintHeader(*outputStream);
    *outputStream << "\n";
  }

  g_tracers.push_back(std::make_tuple(outputStream, tracers));
}

Ptr<L3TrafficTracer>
L3TrafficTracer::Install(Ptr<Node> node, shared_ptr<std::ostream> outputStream)
{
  NS_LOG_DEBUG("Node: " << node->GetId());

  Ptr<L3TrafficTracer> trace = Create<L3TrafficTracer>(outputStream, node);

  return trace;
}

L3TrafficTracer::L3TrafficTracer(shared_ptr<std::ostream> os, Ptr<Node> node)
  : L3Tracer(node)
  , m_os(os)
{
  Ptr<::ns3::ndn::sat::HandoverManager> om = m_nodePtr->GetObject<::ns3::ndn::sat::HandoverManager>();
  if (om != nullptr) {
    om->TraceConnectWithoutContext("ForwardPayloads", MakeCallback(&L3TrafficTracer::ForwardPayloads, this));
  }
}

L3TrafficTracer::L3TrafficTracer(shared_ptr<std::ostream> os, const std::string& node)
  : L3Tracer(node)
  , m_os(os)
{
  Ptr<::ns3::ndn::sat::HandoverManager> om = m_nodePtr->GetObject<::ns3::ndn::sat::HandoverManager>();
  if (om != nullptr) {
    om->TraceConnectWithoutContext("ForwardPayloads", MakeCallback(&L3TrafficTracer::ForwardPayloads, this));
  }
}

L3TrafficTracer::~L3TrafficTracer()
{
  Print(*m_os);
}

void
L3TrafficTracer::PrintHeader(std::ostream& os) const
{
  os << "Node"
     << "\t"
     << "FaceId"
     << "\t"
     << "FaceDescr"
     << "\t"

     << "Prefix" // full name minus last component, show for only "/sin" names
     << "\t"

     << "Type"
     << "\t"
     << "Packets"
     << "\t"
     << "Kilobytes"
     << "\t"
     << "PacketRaw"
     << "\t"
     << "KilobytesRaw";
}

void
L3TrafficTracer::Reset()
{
  for (auto& stats : m_stats) {
    std::get<0>(stats.second).Reset();
    std::get<1>(stats.second).Reset();
  }
}

const double alpha = 0.8;

#define STATS(INDEX) std::get<INDEX>(stats.second)

#define PRINTER(printName, fieldName)                                                              \
  os << m_node << "\t";                                                                            \
  if (stats.first.first != nfd::face::INVALID_FACEID) {                                            \
    os << stats.first.first << "\t";                                                               \
    NS_ASSERT(m_faceInfos.find(stats.first.first) != m_faceInfos.end());                           \
    os << m_faceInfos.find(stats.first.first)->second << "\t";                                     \
    os << stats.first.second << "\t";                                                              \
  }                                                                                                \
  else {                                                                                           \
    os << "-1\tall\t/\t";                                                                          \
  }                                                                                                \
  os << printName << "\t" << STATS(0).fieldName << "\t" << STATS(1).fieldName / 1024.0 << "\n";

void
L3TrafficTracer::Print(std::ostream& os) const
{
  Time time = Simulator::Now();

  for (auto& stats : m_stats) {
    if (stats.first.first == nfd::face::INVALID_FACEID)
      continue;

    PRINTER("InInterests", m_inInterests);
    PRINTER("OutInterests", m_outInterests);

    PRINTER("InData", m_inData);
    PRINTER("OutData", m_outData);

    PRINTER("InNacks", m_inNack);
    PRINTER("OutNacks", m_outNack);

    PRINTER("InSatisfiedInterests", m_satisfiedInterests);
    PRINTER("InTimedOutInterests", m_timedOutInterests);

    PRINTER("OutSatisfiedInterests", m_outSatisfiedInterests);
    PRINTER("OutTimedOutInterests", m_outTimedOutInterests);
  }

  {
    auto i = m_stats.find(make_pair(nfd::face::INVALID_FACEID, Name("/")));
    if (i != m_stats.end()) {
      auto& stats = *i;
      PRINTER("SatisfiedInterests", m_satisfiedInterests);
      PRINTER("TimedOutInterests", m_timedOutInterests);
    }
  }
}

void
L3TrafficTracer::OutInterests(const Interest& interest, const Face& face)
{
  AddInfo(face);
  auto index = make_pair(face.getId(), interest.getName().getPrefix(-1));
  std::get<0>(m_stats[index]).m_outInterests++;
  if (interest.hasWire()) {
    std::get<1>(m_stats[index]).m_outInterests +=
      interest.wireEncode().size();
  }
}

void
L3TrafficTracer::InInterests(const Interest& interest, const Face& face)
{
  AddInfo(face);
  auto index = make_pair(face.getId(), interest.getName().getPrefix(-1));
  std::get<0>(m_stats[index]).m_inInterests++;
  if (interest.hasWire()) {
    std::get<1>(m_stats[index]).m_inInterests +=
      interest.wireEncode().size();
  }
}

void
L3TrafficTracer::OutData(const Data& data, const Face& face)
{
  AddInfo(face);
  auto index = make_pair(face.getId(), data.getName().getPrefix(-1));
  std::get<0>(m_stats[index]).m_outData++;
  if (data.hasWire()) {
    std::get<1>(m_stats[index]).m_outData +=
      data.wireEncode().size();
  }
}

void
L3TrafficTracer::InData(const Data& data, const Face& face)
{
  AddInfo(face);
  auto index = make_pair(face.getId(), data.getName().getPrefix(-1));
  std::get<0>(m_stats[index]).m_inData++;
  if (data.hasWire()) {
    std::get<1>(m_stats[index]).m_inData +=
      data.wireEncode().size();
  }
}

void
L3TrafficTracer::OutNack(const lp::Nack& nack, const Face& face)
{
  AddInfo(face);
  auto index = make_pair(face.getId(), nack.getInterest().getName().getPrefix(-1));
  std::get<0>(m_stats[index]).m_outNack++;
  if (nack.getInterest().hasWire()) {
    std::get<1>(m_stats[index]).m_outNack +=
      nack.getInterest().wireEncode().size();
  }
}

void
L3TrafficTracer::InNack(const lp::Nack& nack, const Face& face)
{
  AddInfo(face);
  auto index = make_pair(face.getId(), nack.getInterest().getName().getPrefix(-1));
  std::get<0>(m_stats[index]).m_inNack++;
  if (nack.getInterest().hasWire()) {
    std::get<1>(m_stats[index]).m_inNack +=
      nack.getInterest().wireEncode().size();
  }
}

void
L3TrafficTracer::SatisfiedInterests(const nfd::pit::Entry& entry, const Face&, const Data&)
{
  std::get<0>(m_stats[make_pair(nfd::face::INVALID_FACEID, Name("/"))]).m_satisfiedInterests++;
  // no "size" stats

  for (const auto& in : entry.getInRecords()) {
    AddInfo(in.getFace());
    auto index = make_pair((in.getFace()).getId(), entry.getName().getPrefix(-1));
    std::get<0>(m_stats[index]).m_satisfiedInterests ++;
  }

  for (const auto& out : entry.getOutRecords()) {
    AddInfo(out.getFace());
    auto index = make_pair((out.getFace()).getId(), entry.getName().getPrefix(-1));
    std::get<0>(m_stats[index]).m_outSatisfiedInterests ++;
  }
}

void
L3TrafficTracer::TimedOutInterests(const nfd::pit::Entry& entry)
{
  std::get<0>(m_stats[make_pair(nfd::face::INVALID_FACEID, Name("/"))]).m_timedOutInterests++;
  // no "size" stats

  for (const auto& in : entry.getInRecords()) {
    AddInfo(in.getFace());
    auto index = make_pair((in.getFace()).getId(), entry.getName().getPrefix(-1));
    std::get<0>(m_stats[index]).m_timedOutInterests++;
  }

  for (const auto& out : entry.getOutRecords()) {
    AddInfo(out.getFace());
    auto index = make_pair((out.getFace()).getId(), entry.getName().getPrefix(-1));
    std::get<0>(m_stats[index]).m_outTimedOutInterests++;
  }
}

void
L3TrafficTracer::ForwardPayloads(const Face& face, const ndn::Block& payload)
{
  AddInfo(face);
  if (payload.type() == ::ndn::tlv::Interest) {
    Interest interest(payload);
    auto index = make_pair(face.getId(), interest.getName().getPrefix(-1));
    std::get<0>(m_stats[index]).m_outInterests++;
  }
  else if (payload.type() == ::ndn::tlv::Data) {
    Data data(payload);
    auto index = make_pair(face.getId(), data.getName().getPrefix(-1));
    std::get<0>(m_stats[index]).m_outData++;
  }
}

void
L3TrafficTracer::AddInfo(const Face& face)
{
  if (m_faceInfos.find(face.getId()) == m_faceInfos.end()) {
    m_faceInfos.insert(make_pair(face.getId(), boost::lexical_cast<std::string>(face.getLocalUri())));
  }
}

} // namespace sat
} // namespace ndn
} // namespace ns3
