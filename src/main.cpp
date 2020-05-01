#include <iostream>

#include <boost/exception/all.hpp>
#include <boost/program_options.hpp>

#include <paracooba/client.hpp>
#include <paracooba/cnf.hpp>
#include <paracooba/communicator.hpp>
#include <paracooba/config.hpp>
#include <paracooba/daemon.hpp>
#include <paracooba/log.hpp>
#include <paracooba/util.hpp>

namespace po = boost::program_options;
using namespace paracooba;

int
main(int argc, char* argv[])
{
  // Start of main.
  auto start = std::chrono::system_clock::now();
  std::shared_ptr<Config> config = std::make_shared<Config>();
  bool continueRunning = config->parseParameters(argc, argv);
  if(!continueRunning) {
    return EXIT_SUCCESS;
  }

  std::shared_ptr<Log> log = std::make_shared<Log>(config);
  auto logger = log->createLogger("main");

  PARACOOBA_LOG(logger, Trace)
    << "Starting paracooba \"" << config->getString(Config::LocalName)
    << "\" in " << (config->isDaemonMode() ? "daemon mode" : "client mode")
    << "with ID = " << config->getId();

  CommunicatorPtr communicator = std::make_shared<Communicator>(config, log);

  communicator->startRunner();

  std::unique_ptr<Client> client;
  std::unique_ptr<Daemon> daemon;

  if(config->isDaemonMode()) {
    daemon = std::make_unique<Daemon>(config, log, communicator);
  } else {
    client = std::make_unique<Client>(config, log, communicator);
    client->solve();
  }

  try {
    communicator->run();
  } catch(const boost::exception& e) {
    PARACOOBA_LOG(logger, LocalError)
      << "Encountered boost exception which was not catched until main()! "
         "Diagnostics info: "
      << boost::diagnostic_information(e);
  } catch(std::exception& e) {
    PARACOOBA_LOG(logger, LocalError)
      << "Encountered exception which was not catched until main()! Message: "
      << e.what();
  }

  int return_code = 0;

  if(!config->isDaemonMode()) {
    // Client mode. There should be some action.
    TaskResult::Status status = client->getStatus();

    try {
      std::string_view dumpTree = config->getString(Config::DumpTreeAtExit);
      if(dumpTree != "") {
        PARACOOBA_LOG(logger, Trace) << "Dump CNF Tree to file " << dumpTree;
        client->getRootCNF()->getCNFTree().dumpTreeToFile(dumpTree);
      }
    } catch(const std::exception& e) {
      PARACOOBA_LOG(logger, LocalError)
        << "Dump CNF Tree to file failed! Error: " << e.what();
    }
    switch(status) {
      case TaskResult::Unsolved:
        std::cout << "unknown" << std::endl;
        break;
       case TaskResult::Satisfiable:
        {
          std::cout << "s SATISFIABLE";
          auto assignement { *client->getSatAssignment() };
          for(auto i = 1; i < assignement.size(); ++i) {
            if(i % 10 == 1) {
              std::cout << "\nv";
            }
            std::cout << " " << (assignement[i] ? "-" : "") << i;
          }
          std ::cout << " 0\n";
          return_code = 10;
        }
        break;
      case TaskResult::Unsatisfiable:
        std::cout << "s UNSATIFIABLE" << std::endl;
        return_code = 20;
        break;
      default:
        std::cout << "s UNKNOWN" << std::endl;
        break;
    }
  } else {
  }

  auto end = std::chrono::system_clock::now();
  PARACOOBA_LOG(logger, Trace) << "Solving took " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "s";

  PARACOOBA_LOG(logger, Trace) << "Ending paracooba.";
  return return_code;
}
