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

#include "timestamp.hpp"

namespace ndn {
namespace kite {

const name::Component Timestamp::TS_COMP("/ts");

Timestamp::Timestamp() = default;

Timestamp::Timestamp(Data data)
  : m_data(std::move(data))
{
  const Block& payload = m_data->getContent();
  payload.parse();

  m_timestamp = name::Component(payload.get(tlv::GenericNameComponent)).toTimestamp();
}

const Data&
Timestamp::toData(const Name& name, const time::milliseconds& freshnessPeriod, KeyChain& keyChain,
                  const ndn::security::SigningInfo& si)
{
  if (!m_data) {
    m_data.emplace(name);

    name::Component tsComp;
    tsComp.fromTimestamp(m_timestamp);

    Block content(tlv::Content);
    content.push_back(tsComp.wireEncode());
    content.encode();
    m_data->setContent(content);
    m_data->setFreshnessPeriod(freshnessPeriod);

    keyChain.sign(*m_data, si);
  }
  return *m_data;
}

void
Timestamp::setTimestamp(const time::system_clock::TimePoint& timestamp)
{
  m_timestamp = timestamp;
  m_data.reset();
}

} // namespace kite
} // namespace ndn
