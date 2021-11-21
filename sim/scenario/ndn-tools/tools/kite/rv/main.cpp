/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015-2019, Arizona Board of Regents.
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
 * @author Eric Newberry <enewberry@email.arizona.edu>
 * @author Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include "core/common.hpp"
#include "core/version.hpp"

#include "rv.hpp"

namespace ndn {
namespace kite {
namespace rv {

namespace po = boost::program_options;

class Runner : noncopyable
{
public:
  explicit
  Runner(const Options& options)
    : m_options(options)
    , m_rv(m_face, m_keyChain, options)
    , m_signalSet(m_face.getIoService(), SIGINT)
  {
    m_signalSet.async_wait([this] (const auto& ec, auto) {
      if (ec != boost::asio::error::operation_aborted) {
        m_rv.stop();
      }
    });
  }

  int
  run()
  {
    std::cout << "KITE RV serving: " << std::endl;
    for(auto prefix : m_options.prefixes) {
      std::cout << "> " << prefix <<std::endl;
    }

    try {
      m_rv.start();
      m_face.processEvents();
    }
    catch (const std::exception& e) {
      std::cerr << "ERROR: " << e.what() << std::endl;
      return 1;
    }

    return 0;
  }

private:
  const Options& m_options;

  Face m_face;
  KeyChain m_keyChain;
  Rv m_rv;

  boost::asio::signal_set m_signalSet;
};

static void
usage(std::ostream& os, const std::string& programName, const po::options_description& options)
{
  os << "Usage: " << programName << " [options] <prefix>\n"
     << "\n"
     << "Starts a NDN ping server that responds to Interests under name ndn:<prefix>/ping\n"
     << "\n"
     << options;
}

static int
main(int argc, char* argv[])
{
  Options options;
  std::string prefix;

  po::options_description visibleDesc("Options");
  visibleDesc.add_options()
    ("help,h",      "print this help message and exit")
    ("version,V",   "print program version and exit")
    ;

  po::options_description hiddenDesc;
  hiddenDesc.add_options()
    ("prefix", po::value<std::string>(&prefix));

  po::positional_options_description posDesc;
  posDesc.add("prefix", -1);

  po::options_description optDesc;
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).positional(posDesc).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }

  if (vm.count("help") > 0) {
    usage(std::cout, argv[0], visibleDesc);
    return 0;
  }

  if (vm.count("version") > 0) {
    std::cout << "KITE RV " << tools::VERSION << std::endl;
    return 0;
  }

  if (prefix.empty()) {
    std::cerr << "ERROR: no name prefix specified\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }
  options.prefixes.push_back(prefix);

  return Runner(options).run();
}

} // namespace rv
} // namespace kite
} // namespace ndn

int
main(int argc, char* argv[])
{
  return ndn::kite::rv::main(argc, argv);
}
