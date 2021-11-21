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
#include "ndn-cxx/util/dummy-client-face.hpp"

#include "tests/boost-test.hpp"
#include "tests/unit/identity-management-time-fixture.hpp"

namespace ndn {
namespace kite {
namespace tests {

class RequestFixture : public ndn::tests::IdentityManagementTimeFixture
{
public:
  RequestFixture()
    : rvPrefix("/rv")
    , producerSuffix("/alice")
  {
  }

protected:
  const Name rvPrefix;
  const Name producerSuffix;
};

BOOST_FIXTURE_TEST_SUITE(TestRequest, RequestFixture)

static Name
makePartialKiteName()
{
  return Name(
    "0711 rv-prefix=/rv 08027276"
    "     keyword 20044B495445"
    "     suffix=/alice 0805616C696365"_block);
}

static Interest
makeRequestInterest()
{
  return Interest(
    "052B 0721 rv-prefix=/rv 08027276"
    "          keyword 20044B495445"
    "          suffix=/alice 0805616C696365"
    "          timestamp 0A0400000000"
    "          nonce 0A0400000000"
    "          signing-components 0800 0800"
    "     2100 can-be-prefix=false"
    "     0A04 nonce 0000 0000"_block);
}

BOOST_AUTO_TEST_CASE(DecodeGood)
{
  Request req(makeRequestInterest());
  BOOST_CHECK_EQUAL(req.getRvPrefix(), rvPrefix);
  BOOST_CHECK_EQUAL(req.getProducerSuffix(), producerSuffix);
  BOOST_CHECK_EQUAL(*req.getTimestamp(), 0_ms);
  BOOST_CHECK_EQUAL(*req.getNonce(), 0);
}

BOOST_AUTO_TEST_CASE(DecodeBad)
{
  Interest interest;
  Name foo("/test");

  // no keyword
  interest.setName(foo);
  BOOST_CHECK_THROW(Request req(interest), std::invalid_argument);

  // no RV prefix
  foo.clear();
  foo.append(KITE_KEYWORD);
  interest.setName(foo);
  BOOST_CHECK_THROW(Request req(interest), std::invalid_argument);

  // no producer suffix
  foo.clear();
  foo.append("test");
  foo.append(KITE_KEYWORD);
  interest.setName(foo);
  BOOST_CHECK_THROW(Request req(interest), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(Encode)
{
  Request req;
  req.setRvPrefix(rvPrefix);
  req.setProducerSuffix(producerSuffix);
  Interest interest = req.makeInterest(m_keyChain);
  BOOST_CHECK_EQUAL(interest.getName().size(), 7);
  BOOST_CHECK_EQUAL(interest.getName().getPrefix(3), makePartialKiteName());
}

BOOST_AUTO_TEST_CASE(EncodeDecodeWithExpiration)
{
  Request req1;
  req1.setRvPrefix(rvPrefix);
  req1.setProducerSuffix(producerSuffix);
  time::milliseconds expiration = 1000_ms;
  req1.setExpiration(expiration);
  BOOST_CHECK_EQUAL(*req1.getExpiration(), expiration);
  Interest interest = req1.makeInterest(m_keyChain);

  Request req2(interest);

  BOOST_CHECK_EQUAL(req2.getRvPrefix(), rvPrefix);
  BOOST_CHECK_EQUAL(req2.getProducerSuffix(), producerSuffix);
  BOOST_CHECK_EQUAL(*req2.getExpiration(), expiration);
}

BOOST_AUTO_TEST_SUITE_END() // TestRequest

} // namespace tests
} // namespace kite
} // namespace ndn
