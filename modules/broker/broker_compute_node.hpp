#pragma once

#include "paracooba/common/types.h"
#include <string>

#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

struct parac_compute_node;
struct parac_message;
struct parac_file;
struct parac_module_solver_instance;
struct parac_handle;

namespace parac {
class NoncopyOStringstream;
}

namespace parac::broker {
class ComputeNodeStore;

class ComputeNode {
  public:
  ComputeNode(parac_compute_node& node,
              parac_handle& handle,
              ComputeNodeStore& store);

  virtual ~ComputeNode();

  struct Description {
    std::string name;
    std::string host;
    uint32_t workers;
    uint16_t udpListenPort;
    uint16_t tcpListenPort;
    bool daemon = true;
    bool local = false;

    Description();
    Description(std::string name,
                std::string host,
                uint32_t workers,
                uint16_t udpListenPort,
                uint16_t tcpListenPort,
                bool daemon,
                bool local = false);

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(name),
         CEREAL_NVP(host),
         CEREAL_NVP(workers),
         CEREAL_NVP(udpListenPort),
         CEREAL_NVP(tcpListenPort),
         CEREAL_NVP(daemon));
    }

    void serializeToMessage(parac_message& msg) const;

    private:
    mutable std::unique_ptr<NoncopyOStringstream> m_descriptionStream;
  };

  struct SolverInstance {
    bool formula_received = false;
    bool formula_parsed = false;
    uint64_t workQueueSize = 0;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(formula_parsed), CEREAL_NVP(workQueueSize));
    }
  };

  struct Status {
    uint64_t workQueueSize() const;
    std::map<parac_id, SolverInstance> solverInstances;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(solverInstances));
    }

    void serializeToMessage(parac_message& msg) const;

    private:
    mutable std::unique_ptr<NoncopyOStringstream> m_statusStream;
  };

  const Description* description() const {
    return m_description ? &m_description.value() : nullptr;
  }
  const Status& status() const { return m_status; }

  void incrementWorkQueueSize(parac_id originator);
  void decrementWorkQueueSize(parac_id originator);
  void formulaParsed(parac_id originator);

  void initDescription(const std::string& name,
                       const std::string& host,
                       uint32_t workers,
                       uint16_t udpListenPort,
                       uint16_t tcpListenPort,
                       bool demon,
                       bool local);

  void applyStatus(const Status& s);

  void receiveMessageFrom(parac_message& msg);
  void receiveMessageDescriptionFrom(parac_message& msg);
  void receiveMessageStatusFrom(parac_message& msg);

  void receiveFileFrom(parac_file& file);

  private:
  parac_compute_node& m_node;
  parac_handle& m_handle;
  ComputeNodeStore& m_store;

  std::optional<Description> m_description;
  Status m_status;
  parac_module_solver_instance* m_solverInstance = nullptr;
};
}
std::ostream&
operator<<(std::ostream& o, const parac::broker::ComputeNode::Description& d);
std::ostream&
operator<<(std::ostream& o,
           const parac::broker::ComputeNode::SolverInstance& si);
std::ostream&
operator<<(std::ostream& o, const parac::broker::ComputeNode::Status& s);
