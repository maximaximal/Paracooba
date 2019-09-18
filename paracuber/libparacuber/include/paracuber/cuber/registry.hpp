#ifndef PARACUBER_CUBER_REGISTRY_HPP
#define PARACUBER_CUBER_REGISTRY_HPP

#include <memory>
#include <vector>
#include "../log.hpp"

namespace paracuber {
class CNF;
namespace cuber {
class Cuber;

class Registry
{
  public:
  explicit Registry(ConfigPtr config, LogPtr log, CNF &rootCNF);
  ~Registry();

  using CuberVector = std::vector<std::unique_ptr<Cuber>>;

  Cuber& getActiveCuber();

  private:
  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  CuberVector m_cubers;
  CNF &m_rootCNF;

  std::vector<int> m_allowanceMap;
};
}
}

#endif
