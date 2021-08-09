#pragma once

#include <paracooba/common/types.h>
#include <paracooba/util/object_manager.hpp>

#include "generic_qbf_handle.hpp"

struct parac_module;

namespace parac::solver_qbf {
class SolverConfig;

class QBFSolverManager : private util::ObjectManager<GenericSolverHandle> {
  public:
  using OM = util::ObjectManager<GenericSolverHandle>;
  using PtrWrapper = OM::PtrWrapper;
  QBFSolverManager(parac_module& mod, Parser& parser, SolverConfig& config);
  ~QBFSolverManager();

  OM::PtrWrapper get(parac_worker worker);

  const SolverConfig& config() const { return m_config; }
  const Parser& parser() const { return m_parser; }

  private:
  parac_module& m_mod;
  Parser& m_parser;
  SolverConfig& m_config;

  std::unique_ptr<GenericSolverHandle> createGenericSolverHandle(size_t idx);
};
}
