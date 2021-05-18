#pragma once

#include <paracooba/util/object_manager.hpp>

#include "generic_qbf_handle.hpp"

struct parac_module;

namespace parac::solver_qbf {
class SolverConfig;

class QBFSolverManager : private util::ObjectManager<GenericSolverHandle> {
  public:
  using ParserPtr = std::unique_ptr<Parser>;
  QBFSolverManager(parac_module& mod, ParserPtr parser, SolverConfig& config);
  ~QBFSolverManager();

  private:
  parac_module& m_mod;
  ParserPtr m_parser;
  SolverConfig& m_config;

  std::unique_ptr<GenericSolverHandle> createGenericSolverHandle(size_t idx);
};
}
