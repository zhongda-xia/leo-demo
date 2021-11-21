/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2019  Regents of the University of California.
 *                          Harbin Institute of Technology
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
 *
 * @author Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

#include "consumer.hpp"

#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"

#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/pointer.h"

#include <ndn-cxx/lp/tags.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.kite.Consumer");

namespace ns3 {
namespace ndn {
namespace kite {

NS_OBJECT_ENSURE_REGISTERED(Consumer);

TypeId
Consumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::kite::Consumer")
      .SetGroupName("Ndn")
      .SetParent<ndn::Consumer>()
      .AddConstructor<Consumer>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&Consumer::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    UintegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeUintegerAccessor(&Consumer::m_seqMax),
                    MakeUintegerChecker<uint32_t>())
    ;

  return tid;
}

Consumer::Consumer()
  : m_frequency(1.0)
  , m_firstTime(true)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

Consumer::~Consumer()
{
}

void
Consumer::StartApplication()
{
  ndn::Consumer::StartApplication();
}

void
Consumer::StopApplication()
{
  ndn::Consumer::StopApplication();
}

void
Consumer::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &ndn::Consumer::SendPacket, this);
    m_firstTime = false;
  }
  else if (!m_sendEvent.IsRunning()) {
    m_sendEvent = Simulator::Schedule(Seconds(1.0 / m_frequency),
                                      &ndn::Consumer::SendPacket, this);
  }
}

void
Consumer::OnData(shared_ptr<const Data> data)
{
  ndn::Consumer::OnData(data);
}

// void
// Consumer::SendPacket()
// {
//   if (!m_active)
//     return;

//   NS_LOG_FUNCTION_NOARGS();

//   uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

//   while (m_retxSeqs.size()) {
//     seq = *m_retxSeqs.begin();
//     m_retxSeqs.erase(m_retxSeqs.begin());
//     break;
//   }

//   if (1 || seq == std::numeric_limits<uint32_t>::max()) {
//     if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
//       if (m_seq >= m_seqMax) {
//         return; // we are totally done
//       }
//     }

//     seq = m_seq++;
//   }

//   shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
//   nameWithSequence->appendSequenceNumber(seq);

//   shared_ptr<Interest> interest = make_shared<Interest>();
//   interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
//   interest->setName(*nameWithSequence);
//   interest->setCanBePrefix(false);
//   time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//   interest->setInterestLifetime(interestLifeTime);

//   // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
//   NS_LOG_INFO("> Interest for " << seq);

//   WillSendOutInterest(seq);

//   m_transmittedInterests(interest, this, m_face);
//   m_appLink->onReceiveInterest(*interest);

//   ScheduleNextPacket();
// }

void
Consumer::OnTimeout(uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION(sequenceNumber);
  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
  // m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

  // m_rtt->IncreaseMultiplier(); // Double the next RTO
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
  // m_retxSeqs.insert(sequenceNumber);
  // ScheduleNextPacket();
}

} // namespace kite
} // namespace ndn
} // namespace ns3
