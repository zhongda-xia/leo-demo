/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2021,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hint-strategy.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/algorithm.hpp"
#include "ns3/ndnSIM/NFD/core/logger.hpp"

namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY(HintStrategy);

NFD_LOG_INIT(HintStrategy);

const time::milliseconds HintStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds HintStrategy::RETX_SUPPRESSION_MAX(250);

HintStrategy::HintStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("HintStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument(
      "HintStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
  this->enableNewNextHopTrigger(true);
}

const Name&
HintStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/hint/%FD%01");
  return strategyName;
}

void
HintStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                   const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  int nEligibleNextHops = 0;

  bool isSuppressed = false;

  for (const auto& nexthop : nexthops) {
    Face& outFace = nexthop.getFace();

    RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream(*pitEntry, outFace);

    if (suppressResult == RetxSuppressionResult::SUPPRESS) {
      NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                    << "to=" << outFace.getId() << " suppressed");
      isSuppressed = true;
      continue;
    }

    if ((outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) ||
        wouldViolateScope(inFace, interest, outFace)) {
      continue;
    }

    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " pitEntry-to=" << outFace.getId());

    if (suppressResult == RetxSuppressionResult::FORWARD) {
      m_retxSuppression.incrementIntervalForOutRecord(*pitEntry->getOutRecord(outFace));
    }
    ++nEligibleNextHops;
  }

  if (nEligibleNextHops == 0 && !isSuppressed) {
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " noNextHop");

    lp::NackHeader nackHeader;
    nackHeader.setReason(lp::NackReason::NO_ROUTE);
    this->sendNack(pitEntry, inFace, nackHeader);

    this->rejectPendingInterest(pitEntry);
  }
}

bool
HintStrategy::isNextHopEligible(const Face& inFace, const Interest& interest,
                                const fib::NextHop& nexthop,
                                const shared_ptr<pit::Entry>& pitEntry,
                                bool wantUnused,
                                time::steady_clock::TimePoint now)
{
  const Face& outFace = nexthop.getFace();

  // do not forward back to the same face, unless it is ad hoc
  if ((outFace.getId() == inFace.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) ||
     (wouldViolateScope(inFace, interest, outFace)))
    return false;

  if (wantUnused) {
    // nexthop must not have unexpired out-record
    auto outRecord = pitEntry->getOutRecord(outFace);
    if (outRecord != pitEntry->out_end() && outRecord->getExpiry() > now) {
      return false;
    }
  }
  return true;
}

void
HintStrategy::afterNewNextHop(const fib::NextHop& nextHop,
                              const shared_ptr<pit::Entry>& pitEntry)
{
  // no need to check for suppression, as it is a new next hop

  auto nextHopFaceId = nextHop.getFace().getId();
  auto& interest = pitEntry->getInterest();

  // try to find an incoming face record that doesn't violate scope restrictions
  for (const auto& r : pitEntry->getInRecords()) {
    auto& inFace = r.getFace();
    if (isNextHopEligible(inFace, interest, nextHop, pitEntry)) {

      NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " pitEntry-to=" << nextHopFaceId << " with hint=" << m_lastSatPrefix);
      Interest newInterest(interest.wireEncode());
      newInterest.refreshNonce();
      DelegationList del;
      del.insert(1, m_lastSatPrefix);
      newInterest.setForwardingHint(del);
      this->sendInterest(pitEntry, nextHop.getFace(), newInterest);

      break; // just one eligible incoming face record is enough
    }
  }

  // if nothing found, the interest will not be forwarded
}

} // namespace fw
} // namespace nfd
