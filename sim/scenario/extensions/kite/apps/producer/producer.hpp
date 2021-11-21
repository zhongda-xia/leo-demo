/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of California, Los Angeles
 *                    Harbin Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * @author Zhongda Xia <xiazhongda@hit.edu.cn>
 */

#ifndef KITE_PRODUCER_WRAPPER_HPP
#define KITE_PRODUCER_WRAPPER_HPP

#include "tools/kite/producer/producer.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

namespace ndn {
namespace kite {
namespace producer {

class SimRunner
{
public:
  SimRunner(ndn::KeyChain& keyChain)
    : m_keyChain(keyChain)
  {
    m_producer = std::make_shared<Producer>(m_face, m_keyChain, m_options);
  }

  void
  addPrefixPair(Name rvPrefix, Name producerSuffix)
  {
    PrefixPair pair;
    pair.rvPrefix = rvPrefix;
    pair.producerSuffix = producerSuffix;
    m_options.prefixPairs.push_back(pair);
  }

  void
  setLifetime(time::milliseconds lifetime)
  {
    m_options.lifetime = lifetime;
  }

  void
  setInterval(time::milliseconds interval)
  {
    m_options.interval = interval;
  }

  void
  run()
  {
    m_producer->start();
  }

  void
  update(const Name& rvPrefix, const Name& producerSuffix)
  {
    m_producer->sendKiteRequest(rvPrefix, producerSuffix);
  }

  void
  updateAll()
  {
    m_producer->sendAllKiteRequests(true);
  }

private:
  Face m_face;
  ndn::KeyChain& m_keyChain;
  Options m_options;
  shared_ptr<Producer> m_producer;
};

} // namespace producer
} // namespace kite
} // namespace ndn

#endif // KITE_PRODUCER_WRAPPER_HPP