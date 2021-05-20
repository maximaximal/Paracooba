#include <paracooba/common/log.h>
#include <paracooba/common/status.h>

extern "C" {
#include <qdpll.h>
}

#include "depqbf_handle.hpp"
#include "parser_qbf.hpp"

namespace parac::solver_qbf {
DepQBFHandle::DepQBFHandle(const Parser& parser)
  : m_parser(parser)
  , m_qdpll(qdpll_create(), &qdpll_delete) {
  addParsedQDIMACS();
}
DepQBFHandle::~DepQBFHandle() {}

void
DepQBFHandle::assumeCube(const CubeIteratorRange& cube) {
  QDPLL* q = m_qdpll.get();
  qdpll_reset(q);
  for(Literal lit : cube) {
    qdpll_assume(q, lit);
  }
}

parac_status
DepQBFHandle::solve() {
  QDPLLResult result = qdpll_sat(m_qdpll.get());
  switch(result) {
    case QDPLL_RESULT_SAT:
      return PARAC_SAT;
    case QDPLL_RESULT_UNSAT:
      return PARAC_UNSAT;
    case QDPLL_RESULT_UNKNOWN:
      return PARAC_UNKNOWN;
  }
  return PARAC_UNKNOWN;
}

void
DepQBFHandle::terminate() {
  qdpll_terminate(m_qdpll.get());
}

static QDPLLQuantifierType
QTypeFromParserQuantifier(Parser::Quantifier q) {
  switch(q.type()) {
    case Parser::Quantifier::EXISTENTIAL:
      return QDPLL_QTYPE_EXISTS;
    case Parser::Quantifier::UNIVERSAL:
      return QDPLL_QTYPE_FORALL;
  }
  return QDPLL_QTYPE_UNDEF;
}

void
DepQBFHandle::addParsedQDIMACS() {
  QDPLL* q = m_qdpll.get();

  // Adjust variable count
  qdpll_adjust_vars(q, m_parser.highestLiteral());

  // Input prefix
  size_t i = 1;
  for(size_t n = 0, nesting = 1; n < m_parser.quantifiers().size();
      n += i, ++nesting) {
    auto qu = m_parser.quantifiers()[n];
    qdpll_new_scope_at_nesting(q, QTypeFromParserQuantifier(qu), nesting);
    qdpll_add(q, qu.alit());

    for(i = 1; n + i < m_parser.quantifiers().size() &&
               m_parser.quantifiers()[n + i].type() == qu.type();
        ++i) {
      qdpll_add(q, m_parser.quantifiers()[n + i].alit());
    }

    qdpll_add(q, 0);
  }

  // Add formula literals
  for(Literal l : m_parser.literals()) {
    qdpll_add(q, l);
  }
}
}
