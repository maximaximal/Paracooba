#include "../include/paracuber/cnf.hpp"

namespace paracuber {
CNF::CNF(uint64_t previous, std::string_view dimacsFile)
  : m_previous(previous)
  , m_dimacsFile(dimacsFile)
{}

CNF::~CNF() {}
}
