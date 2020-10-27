#include "cadical_handle.hpp"
#include "cadical_terminator.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/status.h"

#include <cadical/cadical.hpp>
#include <unistd.h>
#include <vector>

namespace parac::solver {
struct CaDiCaLHandle::Internal {
  Internal(bool& stop)
    : terminator(stop) {}
  CaDiCaL::Solver solver;
  std::vector<int> pregeneratedCubes;
  int vars;
  bool incremental;
  CaDiCaLTerminator terminator;
  std::string path;
};

CaDiCaLHandle::CaDiCaLHandle(bool& stop)
  : m_internal(std::make_unique<Internal>(stop)) {}
CaDiCaLHandle::~CaDiCaLHandle() {}

CaDiCaL::Solver&
CaDiCaLHandle::solver() {
  return m_internal->solver;
}

parac_status
CaDiCaLHandle::parseFile(const std::string& path) {
  m_internal->path = path;

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Start to parse DIMACS file \"{}\".", path);

  const char* parseStatus =
    m_internal->solver.read_dimacs(path.c_str(),
                                   m_internal->vars,
                                   1,
                                   m_internal->incremental,
                                   m_internal->pregeneratedCubes);

  if(parseStatus != 0) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Could not parse DIMACS file \"{}\"! Error: {}",
              path,
              parseStatus);
    return PARAC_PARSE_ERROR;
  }

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Finished parsing DIMACS file \"{}\" with {} variables and {} "
            "pregenerated cubes.",
            path,
            m_internal->vars,
            m_internal->pregeneratedCubes.size());

  return PARAC_OK;
}
}
