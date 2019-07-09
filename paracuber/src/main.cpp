#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include <paracuber/client.hpp>
#include <paracuber/communicator.hpp>
#include <paracuber/config.hpp>
#include <paracuber/daemon.hpp>
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

  PARACUBER_LOG(logger, Trace)
    << "Starting paracuber \"" << config->getString(Config::LocalName) << "\"";

  CommunicatorPtr communicator = std::make_shared<Communicator>(config, log);

  communicator->startRunner();

  Client client(config, log, communicator);
  client.solve();

  communicator->run();

  if(!config->isDaemonMode()) {
    // Client mode. There should be some action.
    TaskResult::Status status = client.getStatus();

    switch(status) {
      case TaskResult::Unsolved:
        PARACUBER_LOG(logger, Info) << "Unsolved!";
        break;
      case TaskResult::Satisfiable:
        PARACUBER_LOG(logger, Info) << "Satisfied!";
        break;
      case TaskResult::Unsatisfiable:
        PARACUBER_LOG(logger, Info) << "Unsatisfiable!";
        break;
    }
  }

  PARACUBER_LOG(logger, Trace) << "Ending paracuber.";
  return EXIT_SUCCESS;
}
