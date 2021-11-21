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

#ifndef NDN_KITE_REQUEST_HPP
#define NDN_KITE_REQUEST_HPP

#include "ndn-cxx/kite/kite-common.hpp"

namespace ndn {
namespace kite {

/** \brief Use a kite::Request object to encode and decode KITE requests.
 *  \sa https://redmine.named-data.net/projects/ndn-tlv/wiki/KITE#KITE-Request
 */
class Request
{
public:
  class Error : public tlv::Error
  {
  public:
    using tlv::Error::Error;
  };

  /** \brief Construct an empty KITE request.
   *  \post getRvPrefix() == "/"
   *  \post getProducerSuffix() == "/"
   *  \post getProducerPrefix() == "/"
   *  \post getExpiration() == nullptr
   */
  Request();

  /** \brief Decode a KITE request from Interest.
   *  \throw std::invalid_argument the Interest is not a KITE request.
   *
   *  The RV prefix, producer suffix, timestamp and nonce are decoded from the name.
   *  The expiration period is decoded from ApplicationParameters.
   */
  explicit
  Request(Interest interest);

  /** \brief Create a new encoded KITE request (a signed Interest).
   *  \param keyChain KeyChain to sign the KITE request.
   *  \param si signing parameters.
   */
  const Interest
  makeInterest(KeyChain& keyChain,
               const security::SigningInfo& si = KeyChain::getDefaultSigningInfo()) const;

  /** \brief Get the RV prefix.
   */
  const Name&
  getRvPrefix() const
  {
    return m_rvPrefix;
  }

  /** \brief Set the RV prefix.
   *  \param rvPrefix the RV prefix.
   *  \post getRvPrefix() == rvPrefix
   */
  Request&
  setRvPrefix(Name rvPrefix);

  /** \brief Get the producer suffix.
   */
  const Name&
  getProducerSuffix() const
  {
    return m_producerSuffix;
  }

  /** \brief Set the producer suffix.
   *  \param producerSuffix the producer suffix.
   *  \post getProducerSuffix() == producerSuffix
   */
  Request&
  setProducerSuffix(Name producerSuffix);

  /** \brief Get the producer prefix.
   *
   *  A producer prefix is the concatenation of an RV prefix and a producer suffix.
   */
  Name
  getProducerPrefix() const
  {
    return Name(m_rvPrefix).append(m_producerSuffix);
  }

  /** \brief Get the timestamp of the decoded KITE request.
   */
  const optional<time::milliseconds>&
  getTimestamp() const
  {
    return m_timestamp;
  }

  /** \brief Get the nonce of the decoded KITE request.
   */
  const optional<uint64_t>&
  getNonce() const
  {
    return m_nonce;
  }

  /** \brief Get the expiration period.
   */
  const optional<time::milliseconds>&
  getExpiration() const
  {
    return m_expiration;
  }

  /** \brief Set the expiration period.
   *  \param expiration the expiration period.
   *  \post *getExpiration() == expiration
   */
  Request&
  setExpiration(const time::milliseconds& expiration);

  /** \brief Check whether a KITE acknowledgment matches this KITE request.
   *
   *  This will check whether the PrefixAnnouncement encoded in the Ack matches
   *  the producer prefix of this Request.
   */
  bool
  canMatch(const Ack& ack) const;

private:
  static std::pair<Name, Name>
  extractPrefixes(const Interest& interest);

private:
  Name m_rvPrefix;
  PartialName m_producerSuffix;
  optional<time::milliseconds> m_timestamp;
  optional<uint64_t> m_nonce;
  optional<time::milliseconds> m_expiration;
};

} // namespace kite
} // namespace ndn

#endif // NDN_KITE_REQUEST_HPP
