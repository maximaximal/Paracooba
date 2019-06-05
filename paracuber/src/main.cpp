#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include <paracuber/config.hpp>
#include <paracuber/log.hpp>

namespace po = boost::program_options;
using namespace paracuber;

int
main(int argc, char* argv[])
{
  std::shared_ptr<Config> config = std::make_shared<Config>();
  bool continueRunning = config->parseParameters(argc, argv);
  if(!continueRunning) {
    return EXIT_SUCCESS;
  }

  std::shared_ptr<Log> log = std::make_shared<Log>(config);
  auto logger = log->createLogger();

  PARACUBER_LOG(logger, Info) << "Starting paracuber \""
                          << config->getString(Config::LocalName) << "\"";

  PARACUBER_LOG(logger, Info) << "Ending paracuber.";
  return EXIT_SUCCESS;
}
