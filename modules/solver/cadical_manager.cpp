#include "cadical_manager.hpp"
#include "cadical_handle.hpp"

#include <paracooba/common/log.h>

namespace parac::solver {
CaDiCaLManager::CaDiCaLManager(parac_module& mod,
                               CaDiCaLHandlePtr parsedFormula)
  : m_mod(mod)
  , m_parsedFormula(std::move(parsedFormula)) {
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Generate CaDiCaLManager for formula in file \"{}\" from {}.",
            m_parsedFormula->path(),
            m_parsedFormula->originatorId());
}
CaDiCaLManager::~CaDiCaLManager() {
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Destroy CaDiCaLManager for formula in file \"{}\" from {}.",
            m_parsedFormula->path(),
            m_parsedFormula->originatorId());
}
}
