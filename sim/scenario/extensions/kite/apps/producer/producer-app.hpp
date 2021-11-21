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

#ifndef KITE_PRODUCER_HPP
#define KITE_PRODUCER_HPP

#include "producer.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/application.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

namespace ns3 {
namespace ndn {
namespace kite {

namespace producer = ::ndn::kite::producer;

// Class inheriting from ns3::Application
class Producer : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("ns3::ndn::kite::Producer")
      .SetParent<Application>()
      .AddConstructor<Producer>()
      .AddAttribute("RvPrefix", "RV prefix", StringValue("/rv"),
                    MakeNameAccessor(&Producer::m_rvPrefix), MakeNameChecker())
      .AddAttribute("ProducerSuffix", "Producer suffix", StringValue("/alice"),
                    MakeNameAccessor(&Producer::m_producerSuffix), MakeNameChecker())
      .AddAttribute("Lifetime", "Lifetime (ms)", UintegerValue(1000),
                    MakeUintegerAccessor(&Producer::m_lifetime),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Interval", "Interval", UintegerValue(1000),
                    MakeUintegerAccessor(&Producer::m_interval),
                    MakeUintegerChecker<uint32_t>());

    return tid;
  }

  Producer()
    : m_active(false)
  {
    Application();
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_active = true;

    // Create an instance of the app, and passing the dummy version of KeyChain (no real signing)
    m_instance.reset(new producer::SimRunner(ndn::StackHelper::getKeyChain()));

    m_instance->addPrefixPair(m_rvPrefix, m_producerSuffix);
    m_instance->setLifetime(time::milliseconds(m_lifetime));
    m_instance->setInterval(time::milliseconds(m_interval));

    m_instance->run(); // can be omitted
  }

  virtual void
  StopApplication()
  {
    m_active = false;

    // Stop and destroy the instance of the app
    m_instance.reset();
  }

public:
  void
  OnAssociation();

private:
  bool m_active;

  Name m_rvPrefix;
  Name m_producerSuffix;
  int m_lifetime;
  int m_interval;
  std::unique_ptr<producer::SimRunner> m_instance;
};

} // namespace kite
} // namespace ndn
} // namespace ns3

#endif // KITE_PRODUCER_HPP
