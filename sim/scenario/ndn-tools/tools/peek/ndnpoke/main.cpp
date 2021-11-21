/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
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
 * @author Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include "ndnpoke.hpp"
#include "core/version.hpp"

#include <ndn-cxx/util/io.hpp>

#include <sstream>

namespace ndn {
namespace peek {

namespace po = boost::program_options;

static void
usage(std::ostream& os, const po::options_description& options)
{
  os << "Usage: ndnpoke [options] ndn:/name\n"
        "\n"
        "Reads payload from stdin and sends it to the local NDN forwarder as a single Data packet\n"
        "\n"
     << options;
}

static int
main(int argc, char* argv[])
{
  PokeOptions options;
  bool wantDigestSha256;

  po::options_description visibleOptDesc;
  visibleOptDesc.add_options()
    ("help,h", "print help and exit")
    ("version,V", "print version and exit")
    ("force,f", po::bool_switch(&options.wantForceData),
        "for, send Data without waiting for Interest")
    ("digest,D", po::bool_switch(&wantDigestSha256),
        "use DigestSha256 signing method instead of SignatureSha256WithRsa")
    ("identity,i", po::value<std::string>(),
        "set identity to be used for signing")
    ("final,F", po::bool_switch(&options.wantLastAsFinalBlockId),
        "set FinalBlockId to the last component of Name")
    ("freshness,x", po::value<int>(),
        "set FreshnessPeriod in milliseconds")
    ("timeout,w", po::value<int>(),
        "set Timeout in milliseconds")
  ;

  po::options_description hiddenOptDesc;
  hiddenOptDesc.add_options()
    ("name", po::value<std::string>(), "Data name");

  po::options_description optDesc;
  optDesc.add(visibleOptDesc).add(hiddenOptDesc);

  po::positional_options_description optPos;
  optPos.add("name", -1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).positional(optPos).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }

  // We store timeout here, instead of PokeOptions, because processEvents is called outside the NdnPoke class
  time::milliseconds timeout = 10_s;

  if (vm.count("help") > 0) {
    usage(std::cout, visibleOptDesc);
    return 0;
  }

  if (vm.count("version") > 0) {
    std::cout << "ndnpoke " << tools::VERSION << std::endl;
    return 0;
  }

  if (vm.count("name") > 0) {
    options.prefixName = vm["name"].as<std::string>();
  }
  else {
    std::cerr << "ERROR: Data name is missing" << std::endl;
    usage(std::cerr, visibleOptDesc);
    return 2;
  }

  if (wantDigestSha256) {
    options.signingInfo.setSha256Signing();
  }

  if (vm.count("identity") > 0) {
    if (wantDigestSha256) {
      std::cerr << "ERROR: Signing identity cannot be specified when using DigestSha256 signing method" << std::endl;
      usage(std::cerr, visibleOptDesc);
      return 2;
    }
    options.signingInfo.setSigningIdentity(vm["identity"].as<std::string>());
  }

  if (vm.count("final") > 0) {
    if (!options.prefixName.empty()) {
      options.wantLastAsFinalBlockId = true;
    }
    else {
      std::cerr << "The provided Name must have 1 or more components to be used with FinalBlockId option" << std::endl;
      usage(std::cerr, visibleOptDesc);
      return 1;
    }
  }

  if (vm.count("freshness") > 0) {
    if (vm["freshness"].as<int>() >= 0) {
      options.freshnessPeriod = time::milliseconds(vm["freshness"].as<int>());
    }
    else {
      std::cerr << "ERROR: FreshnessPeriod must be a non-negative integer" << std::endl;
      usage(std::cerr, visibleOptDesc);
      return 2;
    }
  }

  if (vm.count("timeout") > 0) {
    if (vm["timeout"].as<int>() > 0) {
      timeout = time::milliseconds(vm["timeout"].as<int>());
    }
    else {
      std::cerr << "ERROR: Timeout must a positive integer" << std::endl;
      usage(std::cerr, visibleOptDesc);
      return 2;
    }
  }

  boost::asio::io_service io;
  Face face(io);
  KeyChain keyChain;
  NdnPoke program(face, keyChain, std::cin, options);
  try {
    program.start();
    face.processEvents(timeout);
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n" << std::endl;
    return 1;
  }

  if (program.wasDataSent()) {
    return 0;
  }
  else {
    return 1;
  }
}

} // namespace peek
} // namespace ndn

int
main(int argc, char* argv[])
{
  return ndn::peek::main(argc, argv);
}
