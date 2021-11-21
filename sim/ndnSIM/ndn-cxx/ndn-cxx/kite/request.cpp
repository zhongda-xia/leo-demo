/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2019 Regents of the University of California.
 *                         Harbin Institute of Technology
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 *
 * @author Zhongda Xia <xiazhongda@hit.edu.cn>
 */

#include "ndn-cxx/kite/request.hpp"
#include "ndn-cxx/kite/ack.hpp"
#include "ndn-cxx/security/command-interest-signer.hpp"

namespace ndn {
namespace kite {

Request::Request() = default;

Request::Request(Interest interest)
{
  std::tie(m_rvPrefix, m_producerSuffix) = extractPrefixes(interest);

  m_timestamp = time::milliseconds(interest.getName().at(command_interest::POS_TIMESTAMP).toNumber());
  m_nonce = interest.getName().at(command_interest::POS_RANDOM_VAL).toNumber();

  if (interest.hasParameters()) {
    Block parameter = interest.getParameters();
    parameter.parse();
    Block expirationBlock = parameter.get(tlv::nfd::ExpirationPeriod);
    m_expiration = time::milliseconds(readNonNegativeInteger(expirationBlock));
  }
}

const Interest
Request::makeInterest(KeyChain& keyChain, const security::SigningInfo& si) const
{
  Interest interest(Name(m_rvPrefix).append(KITE_KEYWORD).append(m_producerSuffix));
  interest.setCanBePrefix(false);
  if (m_expiration) {
    Block parameters = makeNonNegativeIntegerBlock(tlv::nfd::ExpirationPeriod, m_expiration->count());
    // The application parameter digest will follow the producer suffix,
    // so as to not mess with command Interest naming convention
    interest.setParameters(parameters);
  }
  security::CommandInterestPreparer preparer;
  interest.setName(preparer.prepareCommandInterestName(interest.getName()));
  keyChain.sign(interest, si);
  return interest;
}

Request&
Request::setRvPrefix(Name rvPrefix)
{
  m_rvPrefix = std::move(rvPrefix);
  return *this;
}

Request&
Request::setProducerSuffix(Name producerSuffix)
{
  m_producerSuffix = std::move(producerSuffix);
  return *this;
}

Request&
Request::setExpiration(const time::milliseconds& expiration)
{
  if (expiration < 0_ms) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("expiration period is negative"));
  }
  m_expiration = expiration;
  return *this;
}

bool
Request::canMatch(const Ack& ack) const
{
  return (ack.getPrefixAnnouncement()->getAnnouncedName() == this->getProducerPrefix());
}


std::pair<Name, Name>
Request::extractPrefixes(const Interest& interest)
{
  Name name(interest.getName());

  auto it = std::find(name.begin(), name.end(), KITE_KEYWORD);
  if (it == name.begin()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("RV prefix not found"));
  }

  if (it == name.end()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("\"KITE\" keyword not found"));
  }

  size_t keywordPos = it - name.begin();

  Name rvPrefix(name.getPrefix(keywordPos));

  Name producerSuffix;
  int producerSuffixLength = name.size() - keywordPos - 1 - command_interest::MIN_SIZE;
  // A KITE request that carries the expiration period will have a ParametersSha256DigestComponent
  // in the name following producer suffix.
  // if (interest.hasParameters())
  //   --producerSuffixLength;

  if (producerSuffixLength > 0)
    producerSuffix = name.getSubName(keywordPos + 1, producerSuffixLength);
  else
    BOOST_THROW_EXCEPTION(std::invalid_argument("producer suffix not found"));

  return {rvPrefix, producerSuffix};
}

} // namespace kite
} // namespace ndn
