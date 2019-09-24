#ifndef PARACUBER_CUBER_REGISTRY_HPP
#define PARACUBER_CUBER_REGISTRY_HPP

#include "../log.hpp"
#include "../readywaiter.hpp"
#include "../cnftree.hpp"
#include <memory>
#include <queue>
#include <vector>

namespace paracuber {
class CNF;
namespace cuber {
class Cuber;

class Registry
{
  public:
  explicit Registry(ConfigPtr config, LogPtr log, CNF& rootCNF);
  ~Registry();

  bool init();

  using CuberVector = std::vector<std::unique_ptr<Cuber>>;
  using AllowanceMap = std::vector<CNFTree::CubeVar>;

  Cuber& getActiveCuber() const;
  inline AllowanceMap& getAllowanceMap() { return m_allowanceMap; }

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
