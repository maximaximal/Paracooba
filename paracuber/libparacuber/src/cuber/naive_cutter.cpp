#include "../../include/paracuber/cuber/naive_cutter.hpp"
#include "../../include/paracuber/cadical_task.hpp"
#include "../../include/paracuber/cnf.hpp"
#include <cadical/cadical.hpp>
#include <vector>

// This is inspired from https://stackoverflow.com/a/2074403
#define FAST_ABS(VAL)                                                     \
  {                                                                       \
    uint32_t temp = VAL >> 31; /* make a mask of the sign bit */          \
    VAL ^= temp;               /* toggle the bits if value is negative */ \
    VAL += temp & 1;           /* add one if value was negative */        \
  }

namespace paracuber {
namespace cuber {

class ClauseIterator : public CaDiCaL::ClauseIterator
{
  public:
  explicit ClauseIterator(NaiveCutter::ClauseMap& m)
    : m_m(m)
  {}
  ~ClauseIterator() {}

  virtual bool clause(const std::vector<int>& clause)
  {
    for(int l : clause) {
      FAST_ABS(l)
      m_m[l] += 1;
    }
    return true;
  }

  private:
  NaiveCutter::ClauseMap& m_m;
};

NaiveCutter::NaiveCutter(ConfigPtr config, LogPtr log, CNF& rootCNF)
  : Cuber(config, log, rootCNF)
  , m_clauseFrequency(rootCNF.getRootTask()->getVarCount() + 1)
{
  // Traverse clauses to build up internal map.
  ClauseMap m(m_clauseFrequency.size(), 0);

  ClauseIterator it(m);
  PARACUBER_LOG(m_logger, Trace) << "Begin traversing CNF clauses for naive "
                                    "cutter literal frequency map. Map Size: "
                                 << m.size();
  m_rootCNF.getRootTask()->getSolver().traverse_clauses(it);
  PARACUBER_LOG(m_logger, Trace)
    << "Finished traversing CNF clauses for naive "
       "cutter literal frequency map. Sorting by value now.";

  PARACUBER_LOG(m_logger, Trace)
    << "Finished sorting by value for naive cutter literal frequency map.";
}
NaiveCutter::~NaiveCutter() {}

CNFTree::CubeVar
NaiveCutter::generateCube(CNFTree::Path path)
{
  return 0;
}
}
}
