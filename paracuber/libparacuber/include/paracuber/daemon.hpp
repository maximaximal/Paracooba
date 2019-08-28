#ifndef PARACUBER_DAEMON_HPP
#define PARACUBER_DAEMON_HPP

#include "cluster-statistics.hpp"
#include "log.hpp"
#include <memory>

namespace paracuber {
class CNF;
class Communicator;

/** @brief Daemonised solver mode that waits for tasks and sends and receives
 * statistics to/from other solvers.
 */
class Daemon
{
  public:
  class Context
  {
    public:
    explicit Context(std::shared_ptr<CNF> rootCNF,
                     int64_t originatorID,
                     Daemon* daemon,
                     ClusterStatistics::Node& statisticsNode);
    ~Context();

    private:
    std::shared_ptr<CNF> m_rootCNF;
    int64_t m_originatorID = 0;
    Daemon* m_daemon;
    Logger m_logger;

    ClusterStatistics::Node& m_statisticsNode;
  };

  /** @brief Constructor
   */
  Daemon(ConfigPtr config,
         LogPtr log,
         std::shared_ptr<Communicator> communicator);
  /** @brief Destructor
   */
  ~Daemon();

  using ContextMap = std::unordered_map<int64_t, std::unique_ptr<Context>>;

  std::pair<Context&, bool> getOrCreateContext(std::shared_ptr<CNF> rootCNF,
                                               int64_t id);

  private:
  std::shared_ptr<Config> m_config;
  ContextMap m_contextMap;
  LogPtr m_log;
  std::shared_ptr<Communicator> m_communicator;
};
}

#endif
