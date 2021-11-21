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

#ifndef KITE_RV_WRAPPER_HPP
#define KITE_RV_WRAPPER_HPP

#include "tools/kite/rv/rv.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

namespace ndn {
namespace kite {
namespace rv {

class SimRunner
{
public:
  SimRunner(ndn::KeyChain& keyChain)
    : m_keyChain(keyChain)
  {
    m_rv = std::make_shared<Rv>(m_face, m_keyChain, m_options);
  }

  void
  addPrefix(Name name)
  {
    m_options.prefixes.push_back(name);
  }

  void
  setServerPrefix(Name name)
  {
    m_rv->m_serverPrefix = name;
  }

  void
  setGlobalPrefix(Name name)
  {
    m_rv->m_globalPrefix = name;
  }

  void
  run()
  {
    m_rv->start();
  }

private:
  Face m_face;
  ndn::KeyChain& m_keyChain;
  Options m_options;
  shared_ptr<Rv> m_rv;
};

} // namespace rv
} // namespace kite
} // namespace ndn

#endif // KITE_RV_WRAPPER_HPP