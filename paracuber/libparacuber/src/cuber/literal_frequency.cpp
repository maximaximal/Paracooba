#include "../../include/paracuber/cuber/literal_frequency.hpp"
#include "../../include/paracuber/cadical_task.hpp"
#include "../../include/paracuber/cnf.hpp"
#include "../../include/paracuber/communicator.hpp"
#include "../../include/paracuber/runner.hpp"
#include "../../include/paracuber/util.hpp"
#include <cadical/cadical.hpp>
#include <vector>

namespace paracuber {
namespace cuber {

class ClauseIterator : public CaDiCaL::ClauseIterator
{
  public:
  explicit ClauseIterator(Cuber::LiteralMap& m)
    : m_m(m)
  {}
  ~ClauseIterator() {}

  virtual bool clause(const std::vector<int>& clause)
  {
    for(int l : clause) {
      m_m[FastAbsolute(l)] += 1;
    }
    return true;
  }

  private:
  Cuber::LiteralMap& m_m;
};

LiteralFrequency::LiteralFrequency(ConfigPtr config,
                                   LogPtr log,
                                   CNF& rootCNF,
                                   Cuber::LiteralMap* allowanceMap)
  : Cuber(config, log, rootCNF)
  , m_literalFrequency(allowanceMap)
{}
LiteralFrequency::~LiteralFrequency() {}

bool
LiteralFrequency::init()
{
  // Frequency map only needs to be built on the client.
  if(m_config->isDaemonMode()) {
    return true;
  }

  *m_literalFrequency =
    LiteralMap(m_rootCNF.getRootTask()->getVarCount() + 1, 0);
  ClauseIterator it(*m_literalFrequency);
  PARACUBER_LOG(m_logger, Trace) << "Begin traversing CNF clauses for naive "
                                    "cutter literal frequency map. Map Size: "
                                 << m_literalFrequency->size() << " elements.";
  m_rootCNF.getRootTask()->getSolver().traverse_clauses(it);
  PARACUBER_LOG(m_logger, Trace)
    << "Finished traversing CNF clauses for"
       " literal frequency map. The map size in RAM is "
    << BytePrettyPrint(m_literalFrequency->size() *
                       sizeof((*m_literalFrequency)[0]))
    << ". Sorting by value now.";

  const size_t logCheck = 10000;
  const size_t abortConditionCheck = 1000;

  auto litIt = m_literalFrequency->begin();
  while(litIt != m_literalFrequency->end()) {
    auto maxIt = std::max_element(litIt, m_literalFrequency->end());
    *maxIt = *litIt;
    *litIt = (maxIt - m_literalFrequency->begin());
    ++litIt;
    if((litIt - m_literalFrequency->begin()) % logCheck == 0) {
      PARACUBER_LOG(m_logger, Trace)
        << "  -> Currently at element " << litIt - m_literalFrequency->begin();
    }

    if((litIt - m_literalFrequency->begin()) % abortConditionCheck == 0) {
      if(!m_config->getCommunicator()->getRunner()->isRunning()) {
        PARACUBER_LOG(m_logger, Trace)
          << "  -> Aborted at element " << litIt - m_literalFrequency->begin()
          << "! (Abort condition checked every " << abortConditionCheck
          << " elements)";
        return false;
      }
    }
  }

  PARACUBER_LOG(m_logger, Trace)
    << "Finished sorting by value for literal frequency map.";
  return true;
}

bool
LiteralFrequency::generateCube(CNFTree::Path path, CNFTree::CubeVar& var)
{
  assert(m_literalFrequency);

  if(CNFTree::getDepth(path) > CNFTree::maxPathDepth) {
    return false;
  }
  auto additionComponent = getAdditionComponent(path);
  auto moduloComponent = getModuloComponent(path);

  auto dec = additionComponent + (moduloComponent - 1);

  if(((float)dec / (float)m_literalFrequency->size()) >=
     m_config->getFloat(Config::FreqCuberCutoff)) {
    return false;
  }

  // The next decision is always determined by the addition component, as no
  // decision must be made. The next decision is therefore always the next most
  // frequent literal.

  if(dec >= m_literalFrequency->size()) {
    return false;
  }

  var = (*m_literalFrequency)[dec];
  return true;
}
}
}
