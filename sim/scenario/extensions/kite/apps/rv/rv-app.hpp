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

#ifndef KITE_RV_HPP
#define KITE_RV_HPP

#include "rv.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/application.h"
#include "ns3/string.h"

namespace ns3 {
namespace ndn {
namespace kite {

namespace rv = ::ndn::kite::rv;

// Class inheriting from ns3::Application
class Rv : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("ns3::ndn::kite::Rv")
      .SetParent<Application>()
      .AddConstructor<Rv>()
      .AddAttribute("Prefix", "RV prefix", StringValue("/rv"),
                    MakeNameAccessor(&Rv::m_prefix), MakeNameChecker())
      .AddAttribute("ServerPrefix", "Server prefix", StringValue("/server"),
                    MakeNameAccessor(&Rv::m_serverPrefix), MakeNameChecker())
      .AddAttribute("GlobalPrefix", "Global prefix", StringValue("/rv"),
                    MakeNameAccessor(&Rv::m_globalPrefix), MakeNameChecker());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    // Create an instance of the app, and passing the dummy version of KeyChain (no real signing)
    m_instance.reset(new rv::SimRunner(ndn::StackHelper::getKeyChain()));

    m_instance->addPrefix(m_prefix);
    m_instance->setServerPrefix(m_serverPrefix);
    m_instance->setGlobalPrefix(m_globalPrefix);

    m_instance->run(); // can be omitted
  }

  virtual void
  StopApplication()
  {
    // Stop and destroy the instance of the app
    m_instance.reset();
  }

private:
  Name m_prefix;
  Name m_serverPrefix;
  Name m_globalPrefix;
  std::unique_ptr<rv::SimRunner> m_instance;
};

} // namespace kite
} // namespace ndn
} // namespace ns3

#endif // KITE_RV_HPP
