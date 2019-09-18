#include "../../include/paracuber/cuber/registry.hpp"
#include "../../include/paracuber/cuber/cuber.hpp"

#include "../../include/paracuber/cuber/literal_frequency.hpp"

namespace paracuber {
namespace cuber {
Registry::Registry(ConfigPtr config, LogPtr log, CNF& rootCNF)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
  , m_rootCNF(rootCNF)
{
  auto litFreqPtr =
    std::make_unique<LiteralFrequency>(config, log, m_rootCNF, &m_allowanceMap);
  LiteralFrequency& litFreq = *litFreqPtr;
  m_cubers.push_back(std::move(litFreqPtr));

  for(auto& cuber : m_cubers) {
    cuber->m_allowanceMap = litFreq.getLiteralFrequency();
  }

  // Now, the allowance map is ready and all waiting callbacks can be called.
  allowanceMapWaiter.setReady(&m_allowanceMap);
}
Registry::~Registry() {}

Cuber&
Registry::getActiveCuber()
{
  return *m_cubers[0];
}
}
}
