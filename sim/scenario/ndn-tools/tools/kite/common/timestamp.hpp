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

#ifndef NDN_TOOLS_KITE_TIMESTAMP_HPP
#define NDN_TOOLS_KITE_TIMESTAMP_HPP

#include "core/common.hpp"

namespace ndn {
namespace kite {

class Timestamp
{
public:
  Timestamp();

  Timestamp(Data data);

  const optional<Data>&
  getData()
  {
    return m_data;
  }

  const Data&
  toData(const Name& name, const time::milliseconds& freshnessPeriod, KeyChain& keyChain,
         const ndn::security::SigningInfo& si = KeyChain::getDefaultSigningInfo());

  void
  setTimestamp(const time::system_clock::TimePoint& timestamp);

  const time::system_clock::TimePoint&
  getTimestamp()
  {
    return m_timestamp;
  }

private:
  optional<Data> m_data;
  time::system_clock::TimePoint m_timestamp;
public:
  static const name::Component TS_COMP;
};

} // namespace kite
} // namespace ndn

#endif // NDN_TOOLS_KITE_TIMESTAMP_HPP
