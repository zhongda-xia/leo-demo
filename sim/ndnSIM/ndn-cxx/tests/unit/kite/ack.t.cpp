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

#include "ndn-cxx/kite/ack.hpp"
#include "ndn-cxx/kite/request.hpp"
#include "ndn-cxx/util/dummy-client-face.hpp"

#include "tests/boost-test.hpp"
#include "tests/unit/identity-management-time-fixture.hpp"

namespace ndn {
namespace kite {
namespace tests {

class AckFixture : public ndn::tests::IdentityManagementTimeFixture
{
public:
  AckFixture()
    : rvPrefix("/rv")
    , producerSuffix("/alice")
  {
  }

  Request
  makeRequest()
  {
    Request req;
    req.setRvPrefix(rvPrefix);
    req.setProducerSuffix(producerSuffix);
    return req;
  }

protected:
  const Name rvPrefix;
  const Name producerSuffix;
};

BOOST_FIXTURE_TEST_SUITE(TestAck, AckFixture)

static Data
makeAckData()
{
  return Data(
    "06C2 0719 rv-prefix=/rv 08027276"
    "          keyword 20044B495445"
    "          suffix=/alice 0805616C696365"
    "          timestamp 0800"
    "          nonce 0800"
    "          signing-components 0800 0800"
    "     1403 content-type=kite-ack 180106"
    "     1579 content:prefix announcement"
    "          0677 0717 announced-name=/rv/alice 08027276 0805616C696365"
    "                    keyword-prefix-ann=20025041 version=0802FD01 segment=08020000"
    "               1403 content-type=prefix-ann 180105"
    "               1530 expire in one hour 6D040036EE80"
    "                    validity FD00FD26 FD00FE0F323031383130333054303030303030"
    "                                      FD00FF0F323031383131323454323335393539"
    "               1603 1B0100 signature"
    "               1720 0000000000000000000000000000000000000000000000000000000000000000"
    "     1603 1B0100 signature"
    "     1720 0000000000000000000000000000000000000000000000000000000000000000"_block);
}

BOOST_AUTO_TEST_CASE(DecodeGood)
{
  Ack ack(makeAckData());
  BOOST_CHECK_EQUAL(ack.getPrefixAnnouncement()->getAnnouncedName(), Name("/rv/alice"));
}

BOOST_AUTO_TEST_CASE(DecodeBad)
{
  Data data;
  Name kiteAckName = makeRequest().makeInterest(m_keyChain).getName();

  // invalid content type
  data.setName(kiteAckName);
  data.setContentType(tlv::ContentType_Key);
  BOOST_CHECK_THROW(Ack kiteAck(data), std::invalid_argument);

  // invalid name
  data.setName("/ndn/unit/test");
  data.setContentType(tlv::ContentType_KiteAck);
  BOOST_CHECK_THROW(Ack kiteAck(data), std::invalid_argument);

  // empty content
  data.setName(kiteAckName);
  data.setContentType(tlv::ContentType_KiteAck);
  BOOST_CHECK_THROW(Ack kiteAck(data), std::invalid_argument);

  // non-empty content with no prefix announcement element
  data.setContent("F000"_block);
  BOOST_CHECK_THROW(Ack kiteAck(data), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(Encode)
{
  Ack ack1;
  Name prefix(rvPrefix);
  prefix.append(producerSuffix);
  ack1.setPrefixAnnouncement(PrefixAnnouncement().setAnnouncedName(prefix));

  Request req = makeRequest();
  Interest interest = req.makeInterest(m_keyChain);
  const Data data = ack1.makeData(interest, m_keyChain);

  BOOST_CHECK_EQUAL(data.getName(), interest.getName());
  BOOST_CHECK(interest.matchesData(data));

  Ack ack2(data);
  BOOST_CHECK_EQUAL(*ack1.getPrefixAnnouncement(), *ack2.getPrefixAnnouncement());
}

BOOST_AUTO_TEST_CASE(EndToEnd)
{
  util::DummyClientFace producerFace(io, m_keyChain); // mobile producer
  util::DummyClientFace forwarderProducerFace(io, m_keyChain); // forwarder face connected to the producer
  util::DummyClientFace rvFace(io, m_keyChain); // RV
  util::DummyClientFace forwarderRvFace(io, m_keyChain); // forwarder face connected to the RV

  producerFace.linkTo(forwarderProducerFace);
  rvFace.linkTo(forwarderRvFace);

  bool receivedAck = false;

  auto kiteAckProcessing = [&] (const Interest& interest, const Data& data) {
                             BOOST_CHECK_NO_THROW(Ack kiteAck(data));
                             Ack ack(data);
                             Request req(interest);
                             BOOST_CHECK(req.canMatch(ack));
                             BOOST_CHECK_EQUAL(interest.getName(), data.getName());
                             receivedAck = true;
                           };
  forwarderProducerFace.setInterestFilter(rvPrefix,
                                          [&] (const InterestFilter&, const Interest& interest) {
                                            // forward to the RV
                                            forwarderRvFace.expressInterest(interest, kiteAckProcessing, nullptr, nullptr);
                                          });

  auto kiteRequestProcessing = [&] (const InterestFilter&, const Interest& interest) {
                                 Request req(interest);
                                 PrefixAnnouncement pa;
                                 pa.setAnnouncedName(req.getProducerPrefix());
                                 Ack kiteAck;
                                 kiteAck.setPrefixAnnouncement(pa);
                                 rvFace.put(kiteAck.makeData(interest, m_keyChain));
                               };
  rvFace.setInterestFilter(rvPrefix, kiteRequestProcessing);

  Request req;
  req.setRvPrefix(rvPrefix);
  req.setProducerSuffix(producerSuffix);

  producerFace.expressInterest(req.makeInterest(m_keyChain), nullptr, nullptr, nullptr);

  advanceClocks(10_ms);

  BOOST_CHECK(receivedAck);
}

BOOST_AUTO_TEST_SUITE_END() // TestAck

} // namespace tests
} // namespace kite
} // namespace ndn
