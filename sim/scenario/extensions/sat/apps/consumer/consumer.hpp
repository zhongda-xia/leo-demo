/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
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

#ifndef SAT_CONSUMER_H
#define SAT_CONSUMER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/apps/ndn-app.hpp"

#include "ns3/random-variable-stream.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"

#include <set>
#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace ns3 {
namespace ndn {
namespace sat {

/**
 * @ingroup ndn-apps
 * \brief NDN application for sending out Interest packets
 */
class Consumer : public App {
public:
  static TypeId
  GetTypeId();

  /**
   * \brief Default constructor
   * Sets up randomizer function and packet sequence number
   */
  Consumer();
  ~Consumer(){};

  // From App
  void
  OnData(shared_ptr<const Data> contentObject);

  // From App
  void
  OnNack(shared_ptr<const ndn::lp::Nack> nack);

  /**
   * @brief Timeout event
   * @param sequenceNumber time outed sequence number
   */
  void
  OnTimeout(uint32_t sequenceNumber);

  /**
   * @brief Actually send packet
   */
  void
  SendNewInterest();

  void
  ResendLastInterest();

public:
  typedef void (*LastRetransmittedInterestDataDelayCallback)(Ptr<App> app, uint32_t seqno, Time delay, int32_t hopCount);
  typedef void (*FirstInterestDataDelayCallback)(Ptr<App> app, uint32_t seqno, Time delay, uint32_t retxCount, int32_t hopCount);
  typedef void (*LostInterestCallback)(Ptr<App> app, uint32_t seqno);

private:
  // from App
  void
  StartApplication();

  void
  StopApplication();

  void
  SendInterest(uint32_t seq);

  void
  WillSendOutInterest(uint32_t sequenceNumber);

private:
  Ptr<UniformRandomVariable> m_rand; ///< @brief nonce generator

  bool m_pending;

  uint32_t m_seq;      ///< @brief currently requested sequence number
  uint32_t m_seqMax;   ///< @brief maximum number of sequence number

  Time m_offTime;          ///< \brief Time interval between packets
  Name m_interestName;     ///< \brief NDN Name of the Interest (use Name)
  Time m_interestLifeTime; ///< \brief LifeTime for interest packet

 /// @cond include_hidden
  /**
   * \struct This struct contains sequence numbers of packets to be retransmitted
   */
  struct RetxSeqsContainer : public std::set<uint32_t> {
  };

  RetxSeqsContainer m_retxSeqs; ///< \brief ordered set of sequence numbers to be retransmitted

  /**
   * \struct This struct contains a pair of packet sequence number and its timeout
   */
  struct SeqTimeout {
    SeqTimeout(uint32_t _seq, Time _time)
      : seq(_seq)
      , time(_time)
    {
    }

    uint32_t seq;
    Time time;
  };
  /// @endcond

  /// @cond include_hidden
  class i_seq {
  };
  class i_timestamp {
  };
  /// @endcond

  /// @cond include_hidden
  /**
   * \struct This struct contains a multi-index for the set of SeqTimeout structs
   */
  struct SeqTimeoutsContainer
    : public boost::multi_index::
        multi_index_container<SeqTimeout,
                              boost::multi_index::
                                indexed_by<boost::multi_index::
                                             ordered_unique<boost::multi_index::tag<i_seq>,
                                                            boost::multi_index::
                                                              member<SeqTimeout, uint32_t,
                                                                     &SeqTimeout::seq>>,
                                           boost::multi_index::
                                             ordered_non_unique<boost::multi_index::
                                                                  tag<i_timestamp>,
                                                                boost::multi_index::
                                                                  member<SeqTimeout, Time,
                                                                         &SeqTimeout::time>>>> {
  };

  SeqTimeoutsContainer m_seqTimeouts; ///< \brief multi-index for the set of SeqTimeout structs

  SeqTimeoutsContainer m_seqLastDelay;
  SeqTimeoutsContainer m_seqFullDelay;
  std::map<uint32_t, uint32_t> m_seqRetxCounts;

  TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */, Time /* delay */, int32_t /*hop count*/>
    m_lastRetransmittedInterestDataDelay;
  TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */, Time /* delay */,
                 uint32_t /*retx count*/, int32_t /*hop count*/> m_firstInterestDataDelay;
  TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */> m_lostInterest;

  /// @endcond
};

} // namespace sat
} // namespace ndn
} // namespace ns3

#endif
