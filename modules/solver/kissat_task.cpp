#include "kissat_task.hpp"

#include <paracooba/common/log.h>
#include <paracooba/module.h>

extern "C" {
const char*
kissat_signal_name(int sig) {
  (void)sig;
  return "CUSTOMSIGNALNAME";
}

#include <kissat/kissat.h>
}

namespace parac::solver {
struct KissatTask::Internal {
  Internal(parac_handle& handle)
    : handle(handle)
    , solver(kissat_init()) {}

  parac_handle& handle;

  kissat* solver;
};

KissatTask::KissatTask(parac_handle& handle, const char* file)
  : m_internal(std::make_unique<Internal>(handle))
  , m_file(file) {}

}
