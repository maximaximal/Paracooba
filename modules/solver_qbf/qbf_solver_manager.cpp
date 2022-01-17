#include <paracooba/common/log.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>

#include "cowsolver_handle.hpp"
#include "depqbf_handle.hpp"
#include "parser_qbf.hpp"
#include "portfolio_qbf_handle.hpp"
#include "qbf_solver_manager.hpp"
#include "solver_qbf_config.hpp"

namespace parac::solver_qbf {
QBFSolverManager::QBFSolverManager(parac_module& mod,
                                   Parser& parser,
                                   SolverConfig& config)
  : m_mod(mod)
  , m_parser(parser)
  , m_config(config) {
  uint32_t workers = 0;

  if(mod.handle && mod.handle->modules[PARAC_MOD_RUNNER]) {
    workers =
      mod.handle->modules[PARAC_MOD_RUNNER]->runner->available_worker_count;
  }

  if(workers > 0) {
    parac_log(
      PARAC_SOLVER,
      PARAC_DEBUG,
      "Generate QBFSolverManager for formula in file \"{}\" from compute node "
      "{} "
      "for {} "
      "workers. Copy operation is deferred to when a solver is requested.",
      m_parser.path(),
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

QBFSolverManager::OM::PtrWrapper
QBFSolverManager::get(parac_worker worker) {
  return OM::get(worker);
}

std::unique_ptr<GenericSolverHandle>
QBFSolverManager::createGenericSolverHandle(size_t idx) {
  PortfolioQBFHandle::SolverHandleFactoryVector vec;
  if(m_config.useDepQBF()) {
    vec.emplace_back(
      [](const Parser& p) { return std::make_unique<DepQBFHandle>(p); });
  }

  for(const auto& cowSolver : m_config.cowSolvers()) {
    vec.emplace_back([this, cowSolver](const Parser& p) {
      return std::make_unique<CowSolverHandle>(m_parser, m_config, cowSolver);
    });
  }

  if(vec.size() == 1) {
    return std::move(vec[0](m_parser));
  } else {
    return std::make_unique<PortfolioQBFHandle>(m_mod, m_parser, vec);
  }

  if(vec.empty()) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Cannot make solver handle for worker {} because no solver was "
              "selected to be used!",
              idx);
    return nullptr;
  }
}
}
