#include "../../include/paracuber/cuber/registry.hpp"
#include "../../include/paracuber/cuber/cuber.hpp"

#include "../../include/paracuber/cuber/literal_frequency.hpp"

namespace paracuber {
namespace cuber {
Registry::Registry(ConfigPtr config, LogPtr log, CNF& rootCNF)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger("Registry"))
  , m_rootCNF(rootCNF)
{}
Registry::~Registry() {}

bool
Registry::init()
{
  m_cubers.clear();

  auto litFreqPtr = std::make_unique<LiteralFrequency>(
    m_config, m_log, m_rootCNF, &m_allowanceMap);
  if(!litFreqPtr->init()) {
    return false;
  }
  LiteralFrequency& litFreq = *litFreqPtr;
  m_cubers.push_back(std::move(litFreqPtr));

  for(auto& cuber : m_cubers) {
    cuber->m_allowanceMap = litFreq.getLiteralFrequency();
  }

  // Now, the allowance map is ready and all waiting callbacks can be called.
  allowanceMapWaiter.setReady(&m_allowanceMap);
  return true;
}

Cuber&
Registry::getActiveCuber() const
{
  return *m_cubers[0];
}

bool
Registry::generateCube(CNFTree::Path path, CNFTree::CubeVar& var)
{
  bool success = getActiveCuber().generateCube(path, var);
  assert(var != 0 || !success);
  return success;
}
}
}
