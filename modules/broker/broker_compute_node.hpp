#pragma once

#include "paracooba/common/types.h"
#include <string>

#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

struct parac_compute_node;

namespace parac::broker {
class ComputeNode {
  public:
  ComputeNode(const parac_compute_node& node);
  ~ComputeNode();

  struct Description {
    const std::string name;
    const std::string host;
    const uint32_t workers;
    const uint16_t udpListenPort;
    const uint16_t tcpListenPort;
    const bool demon;
    const bool local = false;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(name),
         CEREAL_NVP(host),
         CEREAL_NVP(workers),
         CEREAL_NVP(udpListenPort),
         CEREAL_NVP(tcpListenPort),
         CEREAL_NVP(demon));
    }
  };

  struct SolverInstance {
    bool formula_parsed = false;
    uint64_t workQueueSize = 0;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(formula_parsed),
         CEREAL_NVP(workQueueSize));
    }
  };

  struct Status {
    uint64_t workQueueSize() const;
    std::map<parac_id, SolverInstance> solverInstances;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(solverInstances));
    }
  };

  const Description* description() const { return m_description.get(); }
  const Status& status() const { return m_status; }

  void initDescription(const std::string& name,
                       const std::string& host,
                       uint32_t workers,
                       uint16_t udpListenPort,
                       uint16_t tcpListenPort,
                       bool demon,
                       bool local);

  void applyStatus(const Status& s);

  private:
  const parac_compute_node& m_node;

  std::unique_ptr<Description> m_description;
  Status m_status;
};
}
