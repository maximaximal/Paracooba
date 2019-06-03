#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include <paracuber/config.hpp>

namespace po = boost::program_options;
using namespace paracuber;

int
main(int argc, char* argv[])
{
  Config config;
  bool continueRunning = config.parseParameters(argc, argv);
  if(!continueRunning) {
    return EXIT_SUCCESS;
  }

  BOOST_LOG_TRIVIAL(info) << "Starting paracuber \""
                          << config.getString(Config::LocalName) << "\"";

  BOOST_LOG_TRIVIAL(info) << "Ending paracuber.";
  return EXIT_SUCCESS;
}
