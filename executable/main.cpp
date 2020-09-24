#include <cstdlib>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>

#include "CLI.hpp"
#include "ModuleLoader.hpp"

using namespace paracooba;

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

  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Starting ModuleLoader.");
  ModuleLoader loader(thread_registry, config);
  if(!loader.load()) {
    return EXIT_FAILURE;
  }
  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Finished loading modules.");

  cli.parseConfig();

  if(!cli.parseModuleArgs(argc, argv)) {
    return EXIT_SUCCESS;
  }

  loader.pre_init();

  parac_log(PARAC_GENERAL, PARAC_INFO, "Starting Paracooba");

  return EXIT_SUCCESS;
}
