#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>

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

  ThreadRegistryWrapper thread_registry;

  parac_log_init(&thread_registry);

  parac_log(PARAC_GENERAL, PARAC_INFO, "Starting Paracooba");

  ModuleLoader loader;
  loader.load();
}
