#include <paracooba/common/log.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>

#include "depqbf_handle.hpp"
#include "parser_qbf.hpp"
#include "qbf_solver_manager.hpp"
#include "solver_qbf_config.hpp"

namespace parac::solver_qbf {
QBFSolverManager::QBFSolverManager(parac_module& mod,
                                   ParserPtr parser,
                                   SolverConfig& config)
  : m_mod(mod)
  , m_parser(std::move(parser))
  , m_config(config) {
  uint32_t workers = 0;

  if(mod.handle && mod.handle->modules[PARAC_MOD_RUNNER]) {
    workers =
      mod.handle->modules[PARAC_MOD_RUNNER]->runner->available_worker_count;
  }

  if(workers > 0) {
    assert(m_parser);
    parac_log(
      PARAC_SOLVER,
      PARAC_DEBUG,
      "Generate QBFSolverManager for formula in file \"{}\" from compute node "
      "{} "
      "for {} "
      "workers. Copy operation is deferred to when a solver is requested.",
      m_parser->path(),
      config.originatorId(),
      workers);

    ObjectManager<GenericSolverHandle>::init(
      workers, [this](size_t idx) { return createGenericSolverHandle(idx); });
  } else {
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Generate dummy QBFSolverManager for formula that was not parsed "
              "locally, as there are 0 workers.");
  }
}

QBFSolverManager::~QBFSolverManager() {}

std::unique_ptr<GenericSolverHandle>
QBFSolverManager::createGenericSolverHandle(size_t idx) {
  if(m_config.useDepQBF()) {
    return std::make_unique<DepQBFHandle>(*m_parser);
  }
  return nullptr;
}
}
