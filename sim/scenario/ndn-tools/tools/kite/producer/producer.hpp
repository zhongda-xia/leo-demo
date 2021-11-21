/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015-2019, Harbin Institute of Technology.
 *
 * This file is part of ndn-tools (Named Data Networking Essential Tools).
 * See AUTHORS.md for complete list of ndn-tools authors and contributors.
 *
 * ndn-tools is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndn-tools is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndn-tools, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Zhongda Xia <xiazhongda@hit.edu.cn>
 */

#ifndef NDN_TOOLS_KITE_PRODUCER_HPP
#define NDN_TOOLS_KITE_PRODUCER_HPP

#include "core/common.hpp"
#include "tools/kite/common/timestamp.hpp"

namespace ndn {
namespace kite {
namespace producer {

/**
 * @brief RV prefix, producer suffix pair
 */
struct PrefixPair
{
  Name rvPrefix; //!< RV prefix
  Name producerSuffix; //!< producer suffix

  bool
  operator==(const PrefixPair& rhs) const
  {
    return ((this->rvPrefix == rhs.rvPrefix) && (this->producerSuffix == rhs.producerSuffix));
  }
};

/**
 * @brief Options for Producer
 */
struct Options
{
  std::vector<PrefixPair> prefixPairs; //!< mobile producer prefixes
  time::milliseconds lifetime;      //!< Request lifetime
  time::milliseconds interval;      //!< update interval
};

/**
 * @brief KITE mobile producer
 */
class Producer : noncopyable
{
public:
  typedef std::vector<PrefixPair> PrefixPairList;

  Producer(Face& face, KeyChain& keyChain, const Options& options);

  /**
   * @brief Signals when Interest received
   *
   * @param name incoming interest name
   */
  signal::Signal<Producer, Name> afterReceive;

  /**
   * @brief Sets the Interest filter
   *
   * @note This method is non-blocking and caller need to call face.processEvents()
   */
  void
  start();

  /**
   * @brief Unregister set interest filter
   */
  void
  stop();

  /**
   * @brief Send a new KITE request for a prefix
   */
  void
  sendKiteRequest(const PrefixPairList::const_iterator it, bool sendOne = false);

  /**
   * @brief Send a new KITE request for a prefix
   */
  void
  sendKiteRequest(const Name& rvPrefix, const Name& producerSuffix);

  /**
   * @brief Send new KITE requests for all prefixes
   */
  void
  sendAllKiteRequests(bool sendOne = false);

  /**
   * @brief Sync clock with RV
   */
  void
  sendTsRequest(const Name& rvPrefix);

private:
  /**
   * @brief Called when Interest received
   *
   * @param interest incoming Interest
   */
  void
  onInterest(const InterestFilter& filter , const Interest& interest);

  /**
   * @brief Called when potential KiteAck received
   *
   * @param prefix the corresponding producer prefix
   * @param interest sent Interest
   * @param data incoming data
   */
  void
  onKiteAck(const Name prefix, const PrefixPairList::const_iterator it, const Interest& interest, const Data& data);

  /**
   * @brief Called when a Nack is received for a KiteRequest
   */
  void
  onKiteNack(const Name prefix, const PrefixPairList::const_iterator it, const lp::Nack& nack);

  /**
   * @brief Called when a KiteRequest timed out
   */
  void
  onKiteTimeout(const Name prefix, const PrefixPairList::const_iterator it, const Interest& interest);

  /**
   * @brief Called when potential timestamp data received
   *
   * @param prefix the corresponding RV prefix
   * @param interest sent Interest
   * @param data incoming data
   */
  void
  onTsData(const Name rvPrefix, const Interest& interest, const Data& data);

private:
  const Options& m_options;
  Face& m_face;
  KeyChain& m_keyChain;
  std::vector<RegisteredPrefixHandle> m_registeredPrefixes;
  Scheduler m_scheduler;
  // scheduler::EventId m_nextUpdateEvent;
  std::map<Name, EventId> m_nextUpdateEvents;
  time::milliseconds m_clockSkew;
};

} // namespace producer
} // namespace kite
} // namespace ndn

#endif // NDN_TOOLS_KITE_PRODUCER_HPP
