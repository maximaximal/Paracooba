#ifndef PARAC_EXECUTABLE_MODULE_LOADER_HPP
#define PARAC_EXECUTABLE_MODULE_LOADER_HPP

#include <array>
#include <memory>

#include "paracooba/module.h"

struct parac_module_broker;
struct parac_module_runner;
struct parac_module_communicator;
struct parac_module_solver;

namespace paracooba {
class ModuleLoader {
  public:
  ModuleLoader();
  ~ModuleLoader();

  bool load();

  bool isComplete();

  bool hasSolver();
  bool hasRunner();
  bool hasCommunicator();
  bool hasBroker();

  struct parac_module_solver* solver();
  struct parac_module_runner* runner();
  struct parac_module_communicator* communicator();
  struct parac_module_broker* broker();

  parac_handle& handle();

  private:
  static parac_module* prepare(parac_handle* handle, parac_module_type type);

  std::array<std::unique_ptr<parac_module>, PARAC_MOD__COUNT> m_modules;

  parac_handle m_handle;

  bool load(parac_module_type type);
};
}

#endif
