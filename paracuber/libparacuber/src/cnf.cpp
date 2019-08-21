#include "../include/paracuber/cnf.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace paracuber {
CNF::CNF(int64_t previous, std::string_view dimacsFile)
  : m_previous(previous)
  , m_dimacsFile(dimacsFile)
{}

CNF::~CNF() {}
}
