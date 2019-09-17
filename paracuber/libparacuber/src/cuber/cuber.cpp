#include "../../include/paracuber/cuber/cuber.hpp"

namespace paracuber {
namespace cuber {
Cuber::Cuber(ConfigPtr config, LogPtr log, CNF &rootCNF)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
  , m_rootCNF(rootCNF)
{}
Cuber::~Cuber() {}
}
}
