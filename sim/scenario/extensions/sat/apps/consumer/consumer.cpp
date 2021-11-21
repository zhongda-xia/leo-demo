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

#include "consumer.hpp"

#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "ns3/ndnSIM/utils/ndn-ns3-packet-tag.hpp"

#include <ndn-cxx/lp/tags.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.sat.Consumer");

namespace ns3 {
namespace ndn {
namespace sat {

NS_OBJECT_ENSURE_REGISTERED(Consumer);

TypeId
Consumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::sat::Consumer")
      .SetGroupName("Sat")
      .SetParent<App>()
      .AddConstructor<Consumer>()
      .AddAttribute("StartSeq", "Initial sequence number", IntegerValue(0),
                    MakeIntegerAccessor(&Consumer::m_seq), MakeIntegerChecker<int32_t>())

      .AddAttribute("Prefix", "Name of the Interest", StringValue("/"),
                    MakeNameAccessor(&Consumer::m_interestName), MakeNameChecker())
      .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("2s"),
                    MakeTimeAccessor(&Consumer::m_interestLifeTime), MakeTimeChecker())

      .AddTraceSource("LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor(&Consumer::m_lastRetransmittedInterestDataDelay),
                      "ns3::sin::Consumer::LastRetransmittedInterestDataDelayCallback")

      .AddTraceSource("FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor(&Consumer::m_firstInterestDataDelay),
                      "ns3::sin::Consumer::FirstInterestDataDelayCallback")

      .AddTraceSource("LostInterest",
                      "Lost Interest",
                      MakeTraceSourceAccessor(&Consumer::m_lostInterest),
                      "ns3::sin::Consumer::LostInterestCallback");

  return tid;
}

Consumer::Consumer()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_pending(false)
  , m_seq(0)
  , m_seqMax(std::numeric_limits<uint32_t>::max()) // don't request anything
{
  NS_LOG_FUNCTION_NOARGS();
}

// Application Methods
void
Consumer::StartApplication() // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS();

  // do base stuff
  App::StartApplication();
}

void
Consumer::StopApplication() // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS();

  // cleanup base stuff
  App::StopApplication();

  if (m_pending) {
    m_lostInterest(this, m_seq);
  }
}

void
Consumer::SendNewInterest()
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  // BOOST_ASSERT(!m_pending);

  if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
    if (m_seq >= m_seqMax) {
      return; // we are totally done
    }
  }

  if (m_pending) {
    m_lostInterest(this, m_seq);
  }

  uint32_t seq = m_seq++;

  SendInterest(seq);
}

void
Consumer::ResendLastInterest()
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  if (m_seq == 0) {
    BOOST_ASSERT(!m_pending);
    return;
  }

  if (!m_pending) {
    // already satisfied before user link breaks
    NS_LOG_DEBUG("No pending Interest, do not resend");
    return;
  }

  BOOST_ASSERT(m_pending);

  SendInterest(m_seq - 1);
}

void
Consumer::SendInterest(uint32_t seq)
{
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->appendSequenceNumber(seq);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setCanBePrefix(false);
  ::ndn::time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  NS_LOG_INFO("> Interest for " << seq << ", under " << m_interestName);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  m_pending = true;
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
Consumer::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  BOOST_ASSERT(m_pending);

  // This could be a problem......
  uint32_t seq = data->getName().at(-1).toSequenceNumber();
  NS_LOG_INFO("< DATA for " << seq);

  BOOST_ASSERT(seq == m_seq - 1);

  m_pending = false;

  int hopCount = 0;
  auto hopCountTag = data->getTag<::ndn::lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  NS_LOG_DEBUG("Hop count: " << hopCount);

  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find(seq);
  if (entry != m_seqLastDelay.end()) {
    m_lastRetransmittedInterestDataDelay(this, seq, Simulator::Now() - entry->time, hopCount);
  }

  entry = m_seqFullDelay.find(seq);
  if (entry != m_seqFullDelay.end()) {
    m_firstInterestDataDelay(this, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount);
  }

  m_seqRetxCounts.erase(seq);
  m_seqFullDelay.erase(seq);
  m_seqLastDelay.erase(seq);

  m_seqTimeouts.erase(seq);
  m_retxSeqs.erase(seq);
}

void
Consumer::OnNack(shared_ptr<const ::ndn::lp::Nack> nack)
{
  /// tracing inside
  App::OnNack(nack);

  NS_LOG_INFO("NACK received for: " << nack->getInterest().getName()
              << ", reason: " << nack->getReason());

  m_pending = false;
  m_lostInterest(this, m_seq - 1);
}

void
Consumer::OnTimeout(uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION(sequenceNumber);

  BOOST_ASSERT(m_pending);

  m_retxSeqs.insert(sequenceNumber);
}

void
Consumer::WillSendOutInterest(uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
                                << m_seqTimeouts.size() << " items");

  m_seqTimeouts.insert(SeqTimeout(sequenceNumber, Simulator::Now()));
  m_seqFullDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqLastDelay.erase(sequenceNumber);
  m_seqLastDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqRetxCounts[sequenceNumber]++;
}

} // namespace sat
} // namespace ndn
} // namespace ns3
