#ifndef PARAC_EXECUTABLE_MODULE_LOADER_HPP
#define PARAC_EXECUTABLE_MODULE_LOADER_HPP

#include <array>
#include <memory>

#include "paracooba/common/config.h"
#include "paracooba/module.h"

namespace paracooba {
class ModuleLoader {
  public:
  using ModuleArray =
    std::array<std::unique_ptr<parac_module>, PARAC_MOD__COUNT>;

  ModuleLoader(struct parac_thread_registry& thread_registry,
               struct parac_config& config,
               parac_id id);
  ~ModuleLoader();

  bool load();
  bool pre_init();
  bool init();
  bool request_exit();
  bool exit();

  bool isComplete();

  bool hasSolver();
  bool hasRunner();
  bool hasCommunicator();
  bool hasBroker();

  struct parac_module_solver* solver();
  struct parac_module_runner* runner();
  struct parac_module_communicator* communicator();
  struct parac_module_broker* broker();

  parac_handle& handle() { return m_handle; }
  const parac_handle& handle() const { return m_handle; }

  private:
  struct Internal;

  std::unique_ptr<Internal> m_internal;

  static parac_module* prepare(parac_handle* handle, parac_module_type type);

  ModuleArray m_modules;

  parac_handle m_handle;

  bool load(parac_module_type type);
};
}

#endif
