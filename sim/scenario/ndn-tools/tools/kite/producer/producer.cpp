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

#include "producer.hpp"

#include <ndn-cxx/kite/ack.hpp>
#include <ndn-cxx/kite/request.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/logger.hpp>

namespace ndn {
namespace kite {
namespace producer {

NDN_LOG_INIT(kite.producer);

Producer::Producer(Face& face, KeyChain& keyChain, const Options& options)
  : m_options(options)
  , m_face(face)
  , m_keyChain(keyChain)
  , m_scheduler(m_face.getIoService())
{
}

void
Producer::start()
{
  for (auto prefixPair : m_options.prefixPairs) {
    Name producerPrefix(prefixPair.rvPrefix);
    producerPrefix.append(prefixPair.producerSuffix);
    auto handle = m_face.setInterestFilter(
                    producerPrefix,
                    bind(&Producer::onInterest, this, _1, _2),
                    [] (const auto&, const auto& reason) {
                      // NDN_THROW(std::runtime_error("Failed to register prefix: " + reason));
                      BOOST_THROW_EXCEPTION(std::runtime_error("Failed to register prefix: " + reason));
                    });
    m_registeredPrefixes.push_back(handle);
  }
  // sendAllKiteRequests();
}

void
Producer::stop()
{
  m_scheduler.cancelAllEvents();
  for (auto handle : m_registeredPrefixes)
    handle.cancel();
}


void
Producer::onInterest(const InterestFilter& filter, const Interest& interest)
{
  afterReceive(interest.getName());

  NDN_LOG_DEBUG("Received consumer Interest: " << interest.getName()
                << ", under filter: " << filter.getPrefix());

  // if (!interest.getForwardingHint().empty()) {
  //   NDN_LOG_DEBUG("Has hint: " << interest.getForwardingHint()[0].name);
  // }
  // else {
  //   NDN_LOG_DEBUG("No hint.");
  // }

  Name producerPrefix(filter.getPrefix());
  auto data = make_shared<Data>(interest.getName());
  data->setContent(interest.wireEncode());
#ifndef KITE_NDNSIM
  m_keyChain.sign(*data, ndn::security::signingByIdentity(producerPrefix));
#else
  m_keyChain.sign(*data);
#endif
  m_face.put(*data);
}

void
Producer::sendAllKiteRequests(bool sendOne)
{
  m_scheduler.cancelAllEvents();
  const auto& prefixPairs = m_options.prefixPairs;
  for (PrefixPairList::const_iterator it = prefixPairs.begin(); it < prefixPairs.end(); it++) {
    sendKiteRequest(it, sendOne);
  }
}

void
Producer::sendKiteRequest(const PrefixPairList::const_iterator it, bool sendOne)
{
  const auto& prefixPairs = m_options.prefixPairs;
  BOOST_ASSERT(it >= prefixPairs.begin() && it < prefixPairs.end());

  const auto& prefixPair = *it;
  Request req;
  req.setRvPrefix(prefixPair.rvPrefix);
  req.setProducerSuffix(prefixPair.producerSuffix);
  req.setExpiration(m_options.lifetime);
#ifndef KITE_NDNSIM
  Interest interest = req.makeInterest(m_keyChain,
                                       ndn::security::signingByIdentity(req.getProducerPrefix()));
#else
  Interest interest = req.makeInterest(m_keyChain);
#endif

  interest.setInterestLifetime(1000_ms);

  NDN_LOG_DEBUG("Sending Request: " << interest.getName());
  m_face.expressInterest(interest,
                         bind(&Producer::onKiteAck, this, req.getProducerPrefix(), it, _1, _2),
                         bind(&Producer::onKiteNack, this, req.getProducerPrefix(), it, _2),
                         bind(&Producer::onKiteTimeout, this, req.getProducerPrefix(), it, _1));
  if (!sendOne) {
    auto nextUpdateEvent = m_scheduler.scheduleEvent(m_options.interval, [this, it] { sendKiteRequest(it); });
    // m_nextUpdateEvent = m_scheduler.schedule(m_options.interval, [this] { sendKiteRequest(); });
    m_nextUpdateEvents[req.getProducerPrefix()] = nextUpdateEvent;
  }
}

void
Producer::sendKiteRequest(const Name& rvPrefix, const Name& producerSuffix)
{
  PrefixPair prefixPair;
  prefixPair.rvPrefix = rvPrefix;
  prefixPair.producerSuffix = producerSuffix;
  const auto& prefixPairs = m_options.prefixPairs;
  auto it = std::find(prefixPairs.begin(), prefixPairs.end(), prefixPair);
  if (it != prefixPairs.end()) {
    sendKiteRequest(it, true);
  }
  else {
    NDN_LOG_ERROR("Invalid prefix: " << rvPrefix << ", " << producerSuffix);
  }
}

void
Producer::onKiteAck(const Name prefix, const PrefixPairList::const_iterator it, const Interest& interest, const Data& data)
{
  const auto& prefixPairs = m_options.prefixPairs;
  BOOST_ASSERT(it >= prefixPairs.begin() && it < prefixPairs.end());

  // if (data.getContentType() == tlv::ContentType_Nack) {
  //   sendKiteRequest(it);
  //   return;
  // }

  shared_ptr<Ack> ack;
  try {
    ack = make_shared<Ack>(data);
  }
  catch (std::invalid_argument& error) {
    NDN_LOG_ERROR("Invalid KiteAck: " << prefix << ", " << data);
  }

  NDN_LOG_DEBUG("Received Ack for: " << prefix);
}


void
Producer::onKiteNack(const Name prefix, const PrefixPairList::const_iterator it, const lp::Nack& nack)
{
  NDN_LOG_DEBUG("Received NACK: " << nack.getReason() << ", for: " << prefix);
}

void
Producer::onKiteTimeout(const Name prefix, const PrefixPairList::const_iterator it, const Interest& interest)
{
  NDN_LOG_DEBUG("Request timed out: " << interest.getName() << ", for: " << prefix);
}

void
Producer::sendTsRequest(const Name& rvPrefix)
{
  Interest req(Name(rvPrefix).append(Timestamp::TS_COMP));
  req.setMustBeFresh(true);

  NDN_LOG_DEBUG("Sending timestamp request: " << req.getName());

  m_face.expressInterest(req,
                         bind(&Producer::onTsData, this, rvPrefix, _1, _2),
                         nullptr, nullptr);
}

void
Producer::onTsData(const Name rvPrefix, const Interest& interest, const Data& data)
{
  shared_ptr<Timestamp> ts;
  try {
    ts = make_shared<Timestamp>(data);
  }
  catch (std::invalid_argument& error) {
    NDN_LOG_ERROR("Invalid timestamp: " << rvPrefix << ", " << data);
    return;
  }
  m_clockSkew = time::duration_cast<time::milliseconds>(ts->getTimestamp() - time::getUnixEpoch());
}

} // namespace server
} // namespace ping
} // namespace ndn