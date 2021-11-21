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

#ifndef NDN_TOOLS_KITE_RV_HPP
#define NDN_TOOLS_KITE_RV_HPP

#include "core/common.hpp"
#include "tools/kite/common/timestamp.hpp"

#include <ndn-cxx/security/v2/validator.hpp>
#include <ndn-cxx/security/validator-config.hpp>

using ndn::security::v2::ValidationError;

namespace ndn {
namespace kite {
namespace rv {

/**
 * @brief Options for Rv
 */
struct Options
{
  std::vector<Name> prefixes; //!< prefixes to register
  std::string confPath; //!< path to the validator configuration file
  time::milliseconds tsInterval; //!< interval between timestamp generation
};

/**
 * @brief KITE rendezvous server (RV)
 */
class Rv : noncopyable
{
public:
  Rv(Face& face, KeyChain& keyChain, const Options& options);

  /**
   * @brief Signals when Interest received
   *
   * @param name incoming interest name
   */
  signal::Signal<Rv, Name> afterReceive;

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

private:
  /**
   * @brief Called when potential KITE request received
   *
   * @param filter Interest filter
   * @param interest incoming Interest
   */
  void
  onKiteRequest(const InterestFilter& filter, const Interest& interest);

  /**
   * @brief Called when timestamp request received
   *
   * @param filter Interest filter
   * @param interest incoming Interest
   */
  void
  onTsRequest(const InterestFilter& filter, const Interest& interest);

  void
  onConsumerInterest(const InterestFilter& filter, const Interest& interest);

  void
  relayConsumerInterest(const Interest consumerInterest, const Data& data);

  /**
   * @brief Process a potential KITE request
   *
   * @param interest a potential encoded KITE request
   */
  void
  processKiteRequest(const Interest& interest);

  void
  onValidationFailure(const Interest& interest, const ValidationError& error);

private:
  const Options& m_options;
  Face& m_face;
  KeyChain& m_keyChain;
  std::vector<RegisteredPrefixHandle> m_registeredPrefixes;
  ndn::security::ValidatorConfig m_validator;
  kite::Timestamp m_timestamp;

public:
  Name m_globalPrefix;
  Name m_serverPrefix;
  int m_seq;
  int m_qSeq;
};

} // namespace rv
} // namespace kite
} // namespace ndn

#endif // NDN_TOOLS_KITE_RV_HPP
