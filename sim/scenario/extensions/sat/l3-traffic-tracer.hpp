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

#ifndef SAT_TRAFFIC_L3_TRACER_H
#define SAT_TRAFFIC_L3_TRACER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-l3-tracer.hpp"

#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/node-container.h"

#include <map>
#include <list>
#include <tuple>

namespace ns3 {
namespace ndn {
namespace sat {

/**
 * @ingroup ndn-tracers
 * @brief NDN network-layer rate tracer
 */
class L3TrafficTracer : public L3Tracer {
private:
  static const Name s_commonPrefix;
public:
  /**
   * @brief Helper method to install tracers on all simulation nodes
   *
   * @param file File to which traces will be written.  If filename is -, then std::out is used
   *second)
   */
  static void
  InstallAll(const std::string& file);

  /**
   * @brief Helper method to install tracers on the selected simulation nodes
   *
   * @param nodes Nodes on which to install tracer
   * @param file File to which traces will be written.  If filename is -, then std::out is used
   */
  static void
  Install(const NodeContainer& nodes, const std::string& file);

  /**
   * @brief Helper method to install tracers on a specific simulation node
   *
   * @param nodes Nodes on which to install tracer
   * @param file File to which traces will be written.  If filename is -, then std::out is used
   */
  static void
  Install(Ptr<Node> node, const std::string& file);

  /**
   * @brief Explicit request to remove all statically created tracers
   *
   * This method can be helpful if simulation scenario contains several independent run,
   * or if it is desired to do a postprocessing of the resulting data
   */
  static void
  Destroy();

  /**
   * @brief Trace constructor that attaches to the node using node pointer
   * @param os    reference to the output stream
   * @param node  pointer to the node
   */
  L3TrafficTracer(shared_ptr<std::ostream> os, Ptr<Node> node);

  /**
   * @brief Trace constructor that attaches to the node using node name
   * @param os        reference to the output stream
   * @param nodeName  name of the node registered using Names::Add
   */
  L3TrafficTracer(shared_ptr<std::ostream> os, const std::string& node);

  /**
   * @brief Destructor
   */
  virtual ~L3TrafficTracer();

  /**
   * @brief Helper method to install tracers on a specific simulation node
   *
   * @param nodes Nodes on which to install tracer
   * @param outputStream Smart pointer to a stream
   *
   * @returns a tuple of reference to output stream and list of tracers. !!! Attention !!! This
   *tuple needs to be preserved
   *          for the lifetime of simulation, otherwise SEGFAULTs are inevitable
   */
  static Ptr<L3TrafficTracer>
  Install(Ptr<Node> node, shared_ptr<std::ostream> outputStream);

  // from L3Tracer
  virtual void
  PrintHeader(std::ostream& os) const;

  virtual void
  Print(std::ostream& os) const;

protected:
  // from L3Tracer
  virtual void
  OutInterests(const Interest& interest, const Face& face);

  virtual void
  InInterests(const Interest& interest, const Face& face);

  virtual void
  OutData(const Data& data, const Face& face);

  virtual void
  InData(const Data& data, const Face& face);

  virtual void
  OutNack(const lp::Nack& nack, const Face& face);

  virtual void
  InNack(const lp::Nack& nack, const Face& face);

  virtual void
  SatisfiedInterests(const nfd::pit::Entry&, const Face&, const Data&);

  virtual void
  TimedOutInterests(const nfd::pit::Entry&);

  virtual void
  ForwardPayloads(const Face&, const ndn::Block&);

private:

  void
  Reset();

  void
  AddInfo(const Face& face);

private:
  shared_ptr<std::ostream> m_os;

  mutable std::map<std::pair<nfd::FaceId, Name>, std::tuple<Stats, Stats, Stats, Stats>> m_stats;
  std::map<nfd::FaceId, std::string> m_faceInfos; // needed, because face may no longer exists at the time of stat printing
};

} // namespace sat
} // namespace ndn
} // namespace ns3

#endif // SAT_TRAFFIC_L3_TRACER_H
