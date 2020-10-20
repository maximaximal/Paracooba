#include <boost/filesystem/operations.hpp>
#include <csignal>
#include <cstdlib>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>

#include <distrac/distrac.h>

#include "CLI.hpp"
#include "ModuleLoader.hpp"

#define DISTRAC_DEFINITION
#include <distrac_paracooba.h>

using namespace paracooba;

static void
distracInit(distrac_wrapper& wrapper, const CLI& cli) {
  if(cli.isMainNode()) {
    parac_log(
      PARAC_GENERAL, PARAC_DEBUG, "Initializing Distrac as main node...");
  } else {
    parac_log(PARAC_GENERAL, PARAC_DEBUG, "Initializing Distrac...");
  }

  std::string tmpDir = "/dev/shm/" + cli.getLocalName() + "/";
  if(boost::filesystem::exists(tmpDir)) {
    parac_log(PARAC_GENERAL,
              PARAC_LOCALWARNING,
              "Cannot initiate distrac, as the temporary working directory "
              "{} already exists!",
              tmpDir);
    return;
  }
  boost::filesystem::create_directory(tmpDir);

  if(boost::filesystem::exists(cli.getDistracOutput())) {
    parac_log(
      PARAC_GENERAL,
      PARAC_LOCALWARNING,
      "Cannot initiate distrac, as the output tracefile {} already exists!",
      cli.getDistracOutput());
    return;
  }

  wrapper.init(&parac_distrac_definition,
               tmpDir.c_str(),
               cli.getDistracOutput().c_str(),
               cli.getId(),
               cli.getLocalName().c_str(),
               "Paracooba");

  wrapper.is_main_node = cli.isMainNode();

  parac_log(PARAC_GENERAL,
            PARAC_DEBUG,
            "Successfully initialized distrac! Trace will be generated.");
}

static bool
isDirectoryEmpty(std::string path) {
  if(!boost::filesystem::is_directory(path))
    return false;

  boost::filesystem::directory_iterator end;
  boost::filesystem::directory_iterator it(path);
  if(it == end)
    return true;
  else
    return false;
}

static ModuleLoader* GlobalModuleLoader = nullptr;

static void
InterruptHandler(int s) {
  static bool exitRequested = false;
  if(s == SIGINT && !exitRequested) {
    exitRequested = true;
    GlobalModuleLoader->request_exit();
  }
}

int
main(int argc, char* argv[]) {
  // Workaround for wonky locales.
  try {
    std::locale loc("");
  } catch(const std::exception& e) {
    setenv("LC_ALL", "C", 1);
  }

  // Workaround for empty args.
  static char* argv_default[] = { (char*)"", nullptr };
  if(argc == 0 && argv == nullptr) {
    argc = 1;
    argv = argv_default;
  }

  ThreadRegistryWrapper thread_registry;
  ConfigWrapper config;
  CLI cli(config);

  parac_log_init(&thread_registry);

  if(!cli.parseGlobalArgs(argc, argv)) {
    return EXIT_SUCCESS;
  }

  ModuleLoader loader(thread_registry,
                      config,
                      cli.getId(),
                      cli.getLocalName().c_str(),
                      cli.getHostName().c_str(),
                      cli.getInputFile().c_str());
  GlobalModuleLoader = &loader;
  signal(SIGINT, InterruptHandler);

  distrac_wrapper distracWrapper;
  if(cli.distracEnabled()) {
    distracInit(distracWrapper, cli);
    loader.handle().distrac = &distracWrapper;
  }

  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Starting ModuleLoader.");
  if(!loader.load()) {
    return EXIT_FAILURE;
  }
  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Finished loading modules.");

  cli.parseConfig();

  if(!cli.parseModuleArgs(argc, argv)) {
    return EXIT_SUCCESS;
  }

  if(cli.getInputFile() != "" &&
     !boost::filesystem::exists(cli.getInputFile())) {
    parac_log(PARAC_GENERAL,
              PARAC_FATAL,
              "Input file \"{}\" does not exist!",
              cli.getInputFile());
    return EXIT_FAILURE;
  }

  bool status = loader.pre_init();

  if(!status) {
    parac_log(PARAC_GENERAL,
              PARAC_FATAL,
              "pre_init did not complete successfully with all modules!");
    return EXIT_FAILURE;
  }

  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Initializing Paracooba.");

  // Init also calls init of all modules, which may spawn threads in the
  // thread_registry which are also solved.
  status = loader.init();

  if(!status) {
    parac_log(PARAC_GENERAL,
              PARAC_FATAL,
              "init did not complete successfully with all modules!");
    return EXIT_FAILURE;
  }

  parac_log(PARAC_GENERAL,
            PARAC_DEBUG,
            "Fully initialized Paracooba, modules are running.");

  thread_registry.wait();

  parac_log(PARAC_GENERAL, PARAC_INFO, "All threads exited, ending Paracooba.");

  if(cli.distracEnabled()) {
    distracWrapper.finalize(loader.handle().offsetNS);

    if(isDirectoryEmpty(distracWrapper.working_directory)) {
      boost::filesystem::remove(distracWrapper.working_directory);
    } else {
      parac_log(PARAC_GENERAL,
                PARAC_LOCALWARNING,
                "Cannot remove temporary working directory \"{}\""
                "of distrac, as it is not empty!",
                distracWrapper.working_directory);
    }
  }

  return EXIT_SUCCESS;
}
