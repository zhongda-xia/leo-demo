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

#include "rv.hpp"

#include <ndn-cxx/kite/ack.hpp>
#include <ndn-cxx/kite/request.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/logger.hpp>

namespace ndn {
namespace kite {
namespace rv {

NDN_LOG_INIT(kite.rv);

Rv::Rv(Face& face, KeyChain& keyChain, const Options& options)
  : m_options(options)
  , m_face(face)
  , m_keyChain(keyChain)
  , m_validator(face)

  , m_seq(0)
  , m_qSeq(0)
{
#ifndef KITE_NDNSIM
  m_validator.load(m_options.confPath);
#endif
}

void
Rv::start()
{
  for(auto prefix : m_options.prefixes) {
    auto registeredPrefix = m_face.registerPrefix(prefix,
                            [] (const auto& prefix) {
                              NDN_LOG_DEBUG("Prefix registered successfully: " << prefix);
                            },
                            [] (const auto&, const auto& reason) {
                              // NDN_THROW(std::runtime_error("Failed to register prefix: " + reason));
                              BOOST_THROW_EXCEPTION(std::runtime_error("Failed to register prefix: " + reason));
                            });
    m_registeredPrefixes.push_back(registeredPrefix);
    m_face.setInterestFilter(InterestFilter(Name(prefix).append(kite::KITE_KEYWORD)),
                                            std::bind(&Rv::onKiteRequest, this, _1, _2));
    m_face.setInterestFilter(InterestFilter(Name(prefix).append("/ts")), std::bind(&Rv::onKiteRequest, this, _1, _2));
    // for other Interests, supposedly consumer Interests that shouldn't come here if trace is present
    m_face.setInterestFilter(InterestFilter(Name(prefix)), std::bind(&Rv::onConsumerInterest, this, _1, _2));
  }
}

void
Rv::stop()
{
  for (auto registeredPrefix : m_registeredPrefixes)
    registeredPrefix.cancel();
}


void
Rv::onKiteRequest(const InterestFilter& filter, const Interest& interest)
{
  afterReceive(interest.getName());

#ifndef KITE_NDNSIM
  m_validator.validate(interest,
                       std::bind(&Rv::processKiteRequest, this, _1),
                       std::bind(&Rv::onValidationFailure, this, _1, _2));
#else
  processKiteRequest(interest);
#endif
}

void
Rv::processKiteRequest(const Interest& interest)
{
  NDN_LOG_DEBUG("Verification success for: " << interest.getName());

  shared_ptr<Request> req;
  try {
    req = make_shared<Request>(interest);
  }
  catch (std::invalid_argument e) {
    NDN_LOG_ERROR("Invalid KITE request: " << interest.getName());
    return;
  }

  Ack ack;
  PrefixAnnouncement pa;
  pa.setAnnouncedName(req->getProducerPrefix());
  if (req->getExpiration()) {
    NDN_LOG_DEBUG("Has expiration: " << std::to_string(req->getExpiration()->count()) << " ms");
    pa.setExpiration(*req->getExpiration());
  }
  else {
    NDN_LOG_DEBUG("Expiration not set, setting to 1000 ms...");
    pa.setExpiration(1000_ms);
  }

  ack.setPrefixAnnouncement(pa);

#ifndef KITE_NDNSIM
  m_face.put(ack.makeData(interest, m_keyChain, ndn::security::signingByIdentity(m_options.prefixes[0])));
#else
  m_face.put(ack.makeData(interest, m_keyChain));
#endif

  // update mapping
  if (0) {
    Name locator(m_globalPrefix);
    shared_ptr<Name> nameWithSequence = make_shared<Name>(m_serverPrefix);
    nameWithSequence->append("update");
    nameWithSequence->append(locator);
    // nameWithSequence->appendSequenceNumber(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    nameWithSequence->appendSequenceNumber(m_seq++);

    shared_ptr<Interest> interest = make_shared<Interest>();
    // interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    interest->setName(*nameWithSequence);
    time::milliseconds interestLifetime(2000);
    interest->setInterestLifetime(interestLifetime);
    interest->setCanBePrefix(false);

    NDN_LOG_INFO("Send mapping update: " << interest->getName());

    m_face.expressInterest(*interest,
                           [this] (auto&&, const auto& data) { ; },
                           [this] (auto&&, const auto& nack) { ; },
                           [this] (auto&&) { ; }
                           );
  }
}

void
Rv::onValidationFailure(const Interest& interest, const ValidationError& error)
{
  NDN_LOG_DEBUG("Verification failure for: " << interest.getName() << "\nError: " << error);
}

void
Rv::onTsRequest(const InterestFilter& filter, const Interest& interest)
{
  afterReceive(interest.getName());

  auto elapsedTime =
    time::duration_cast<time::milliseconds>(time::getUnixEpoch() - m_timestamp.getTimestamp());

  if (!m_timestamp.getData() || elapsedTime >= m_options.tsInterval) {
    m_timestamp.setTimestamp(time::getUnixEpoch());
  }

#ifndef KITE_NDNSIM
  auto identity = filter.getPrefix().getPrefix(-1); // remove "/ts"
  m_face.put(m_timestamp.toData(filter.getPrefix(), m_options.tsInterval, m_keyChain,
                                ndn::security::signingByIdentity(identity)));
#else
  m_face.put(m_timestamp.toData(filter.getPrefix(), m_options.tsInterval, m_keyChain));
#endif
}

void
Rv::onConsumerInterest(const InterestFilter& filter, const Interest& interest)
{
  if (interest.getName()[1] == (kite::KITE_KEYWORD)) {
    NDN_LOG_INFO("KITE request, do not relay.");
    return;
  }

  auto delegations = interest.getForwardingHint();
  if (!delegations.empty()) {
    // has hint, do nothing
    NDN_LOG_INFO("Has hint: " << delegations[0].name << " " << m_globalPrefix);
    // BOOST_ASSERT(delegations[0].name == m_globalPrefix);
    return;
  }

  // query the current attachment and relay
  if (0) {
    shared_ptr<Name> nameWithSequence = make_shared<Name>(m_serverPrefix);
    nameWithSequence->append(m_globalPrefix);
    nameWithSequence->appendSequenceNumber(m_qSeq++);

    shared_ptr<Interest> qInterest = make_shared<Interest>();
    // qInterest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    qInterest->setName(*nameWithSequence);
    time::milliseconds interestLifeTime(2000_ms);
    qInterest->setInterestLifetime(interestLifeTime);
    qInterest->setCanBePrefix(false);

    NDN_LOG_INFO("> Mapping Interest for " << m_qSeq - 1);
    m_face.expressInterest(*qInterest,
                           bind(&Rv::relayConsumerInterest, this, interest, _2),
                           [this] (auto&&, const auto& nack) { ; },
                           [this] (auto&&) { ; }
                           );
  }
}

void
Rv::relayConsumerInterest(const Interest consumerInterest, const Data& data)
{
  uint32_t seq = data.getName().at(-1).toSequenceNumber();
  NDN_LOG_INFO("> Mapping Data for " << seq);
  Block content(data.getContent().value(), data.getContent().value_size());
  Name locator(content);
  NDN_LOG_INFO("New locator: " << locator);
  if (locator == m_globalPrefix) {
    NDN_LOG_INFO("Local, do not relay, implies no trace");
    return;
  }
  if (locator == "/") {
    NDN_LOG_INFO("Empty locator, do not relay.");
    return;
  }
  Interest newInterest(consumerInterest.wireEncode());
  DelegationList hint;
  hint.insert(1, locator);
  newInterest.setForwardingHint(hint);
  newInterest.refreshNonce(); // change nonce to avoid being considered a dup
  NDN_LOG_INFO("Relay: " << newInterest);
  m_face.expressInterest(newInterest,
                         [this] (auto&&, const auto& data) { ; },
                         [this] (auto&&, const auto& nack) { ; },
                         [this] (auto&&) { ; }
                         );
}

} // namespace server
} // namespace ping
} // namespace ndn
