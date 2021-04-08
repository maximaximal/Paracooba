#include <boost/filesystem/operations.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>

#ifdef ENABLE_DISTRAC
#include <distrac/distrac.h>
#include <distrac/headers.h>

#define DISTRAC_DEFINITION
#include <distrac_paracooba.h>
#endif

#include "CLI.hpp"
#include "paracooba/common/status.h"
#include <paracooba/loader/ModuleLoader.hpp>

using namespace paracooba;

#ifdef ENABLE_DISTRAC
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
#endif

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

struct ProgramRuntimeHelper {
  ProgramRuntimeHelper() {}
  ~ProgramRuntimeHelper() {
    // Printing inspired from https://stackoverflow.com/a/22069038
    using namespace std::chrono;
    using day_t = duration<long, std::ratio<3600 * 24>>;

    auto end = high_resolution_clock::now();

    auto dur = end - start;
    auto d = duration_cast<day_t>(dur);
    auto h = duration_cast<hours>(dur -= d);
    auto m = duration_cast<minutes>(dur -= h);
    auto s = duration_cast<seconds>(dur -= m);

    auto totalSeconds = duration_cast<duration<float>>(end - start);

    parac_log(PARAC_GENERAL,
              PARAC_DEBUG,
              "Wall-clock runtime: {}s ({}d, {}h, {}min, {}s)",
              totalSeconds.count(),
              d.count(),
              h.count(),
              m.count(),
              s.count());
  }

  std::chrono::high_resolution_clock::time_point start =
    std::chrono::high_resolution_clock::now();
};

int
main(int argc, char* argv[]) {
  // Workaround for wonky locales.
  try {
    std::locale loc("");
  } catch(const std::exception& e) {
    setenv("LC_ALL", "C", 1);
  }

  ProgramRuntimeHelper runtimeHelper;

  // Workaround for empty args.
  static char* argv_default[] = { (char*)"", nullptr };
  if(argc == 0 && argv == nullptr) {
    argc = 1;
    argv = argv_default;
  }

  ThreadRegistryWrapper thread_registry(0);
  ConfigWrapper config;
  CLI cli(config);

  if(!cli.parseGlobalArgs(argc, argv)) {
    return EXIT_SUCCESS;
  }

  thread_registry.belongs_to_id = cli.getId();
  parac_log_init(&thread_registry);

  ModuleLoader loader(thread_registry,
                      config,
                      cli.getId(),
                      cli.getLocalName().c_str(),
                      cli.getHostName().c_str());
  GlobalModuleLoader = &loader;
  signal(SIGINT, InterruptHandler);

  // Sigpipe would exit the whole application! Don't do that. Errors are handled
  // in modules directly.
  signal(SIGPIPE, SIG_IGN);

#ifdef ENABLE_DISTRAC
  distrac_wrapper distracWrapper;
  if(cli.distracEnabled()) {
    distracInit(distracWrapper, cli);
    loader.handle().distrac = &distracWrapper;
  }
#endif

  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Starting ModuleLoader.");
  if(!loader.load()) {
    return EXIT_FAILURE;
  }
  parac_log(PARAC_GENERAL, PARAC_DEBUG, "Finished loading modules.");

  cli.parseConfig();

  if(!cli.parseModuleArgs(argc, argv)) {
    return EXIT_SUCCESS;
  }

  const char* inputFile = nullptr;
  if(cli.getInputFile() != "") {
    inputFile = cli.getInputFile().c_str();

    if(loader.handle().distrac) {
#ifdef ENABLE_DISTRAC
      strncpy(loader.handle().distrac->definition.file_header.problem_name,
              inputFile,
              DISTRAC_LONGNAME_LENGTH);
      loader.handle().distrac->is_main_node = true;
#else
      parac_log(PARAC_GENERAL,
                PARAC_LOCALERROR,
                "Cannot initialize distrac, as distrac support is not compiled "
                "into binary! Enable using -DENABLE_DISTRAC");
#endif
    }
  }
  loader.handle().input_file = inputFile;

  if(cli.getInputFile() != "" && cli.getInputFile()[0] != ':' &&
     cli.getInputFile() != "-" &&
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

  parac_log(
    PARAC_GENERAL, PARAC_DEBUG, "All threads exited, ending Paracooba.");

#ifdef ENABLE_DISTRAC
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
#endif

  // Only client nodes give a status at the end.
  if(loader.handle().input_file) {
    switch(loader.handle().exit_status) {
      case PARAC_SAT:
        std::cout << "s SATISFIABLE";
        for(auto i = 1; i <= loader.handle().assignment_highest_literal(
                               loader.handle().assignment_data);
            ++i) {
          if(i % 10 == 1) {
            std::cout << "\nv";
          }
          std::cout << " "
                    << (loader.handle().assignment_is_set(
                          loader.handle().assignment_data, i)
                          ? "-"
                          : "")
                    << i;
        }
        std ::cout << " 0" << std::endl;
        break;
      case PARAC_UNSAT:
        std::cout << "s UNSATISFIABLE" << std::endl;
        break;
      case PARAC_UNKNOWN:
      case PARAC_ABORTED:
      default:
        std::cout << "s UNKNOWN" << std::endl;
        break;
    }
  }

  return EXIT_SUCCESS;
}
