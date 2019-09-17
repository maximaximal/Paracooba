#include "../../include/paracuber/cuber/registry.hpp"
#include "../../include/paracuber/cuber/cuber.hpp"

#include "../../include/paracuber/cuber/naive_cutter.hpp"

namespace paracuber {
namespace cuber {
Registry::Registry(ConfigPtr config, LogPtr log, CNF &rootCNF)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
  , m_rootCNF(rootCNF)
{
  m_cubers.push_back(std::make_unique<NaiveCutter>(config, log, m_rootCNF));
}
Registry::~Registry() {}

Cuber&
Registry::getActiveCuber()
{
  return *m_cubers[0];
}
}
}
