#ifndef PARACUBER_CUBER_REGISTRY_HPP
#define PARACUBER_CUBER_REGISTRY_HPP

#include "../log.hpp"
#include "../readywaiter.hpp"
#include <memory>
#include <vector>
#include <queue>

namespace paracuber {
class CNF;
namespace cuber {
class Cuber;

class Registry
{
  public:
  explicit Registry(ConfigPtr config, LogPtr log, CNF& rootCNF);
  ~Registry();

  using CuberVector = std::vector<std::unique_ptr<Cuber>>;
  using AllowanceMap = std::vector<int>;

  Cuber& getActiveCuber();

  ReadyWaiter<AllowanceMap> allowanceMapWaiter;

  private:
  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  CuberVector m_cubers;
  CNF& m_rootCNF;

  AllowanceMap m_allowanceMap;
};
}
}

#endif
