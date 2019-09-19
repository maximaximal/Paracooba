#ifndef PARACUBER_DAEMON_HPP
#define PARACUBER_DAEMON_HPP

#include "cluster-statistics.hpp"
#include "log.hpp"
#include <memory>
#include <mutex>
#include <shared_mutex>

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

    enum State
    {
      JustCreated = 0b00000000u,
      FormulaReceived = 0b00000001u,
      AllowanceMapReceived = 0b00000010u,
      FormulaParsed = 0b00000100u,
      WaitingForWork = 0b00001000u
    };

    void start(State change);
    std::shared_ptr<CNF> getRootCNF() { return m_rootCNF; }

    private:
    std::shared_ptr<CNF> m_rootCNF;
    int64_t m_originatorID = 0;
    Daemon* m_daemon;
    Logger m_logger;

    State m_state = JustCreated;

    std::mutex m_contextMutex;

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

  Context* getContext(int64_t id);

  std::pair<Context&, bool> getOrCreateContext(int64_t id);

  private:
  std::shared_ptr<Config> m_config;
  ContextMap m_contextMap;
  std::shared_mutex m_contextMapMutex;
  LogPtr m_log;
  std::shared_ptr<Communicator> m_communicator;
};

inline Daemon::Context::State
operator|(Daemon::Context::State a, Daemon::Context::State b)
{
  return static_cast<Daemon::Context::State>(static_cast<int>(a) |
                                             static_cast<int>(b));
}
inline Daemon::Context::State operator&(Daemon::Context::State a,
                                        Daemon::Context::State b)
{
  return static_cast<Daemon::Context::State>(static_cast<int>(a) &
                                             static_cast<int>(b));
}
}

#endif
