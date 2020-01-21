#ifndef PARACUBER_MESSAGES_CNFTREE_NODE_STATUS_REPLY
#define PARACUBER_MESSAGES_CNFTREE_NODE_STATUS_REPLY

#include <cstdint>
#include <optional>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

namespace paracuber {
namespace messages {
class CNFTreeNodeStatusReply
{
  public:
  CNFTreeNodeStatusReply() {}
  CNFTreeNodeStatusReply(int64_t handle, uint64_t path, int64_t cnfId)
    : handle(handle)
    , path(path)
    , cnfId(cnfId)
  {}
  virtual ~CNFTreeNodeStatusReply() {}

  struct Node
  {
    uint8_t state;
    int32_t literal;
  };

  using NodeVector = std::vector<Node>;

  int64_t getHandle() const { return handle; }
  uint64_t getPath() const { return path; }
  int64_t getCnfId() const { return cnfId; }
  const NodeVector& getNodes() const { return nodes; }

  void addNode(uint8_t state, int32_t literal)
  {
    nodes.push_back({ state, literal });
  }

  size_t getNodeSize() const { return nodes.size(); }

  private:
  friend class cereal::access;

  int64_t handle;
  uint64_t path;
  int64_t cnfId;
  NodeVector nodes;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(handle),
       CEREAL_NVP(path),
       CEREAL_NVP(cnfId),
       CEREAL_NVP(nodes));
  }
};
}
}

#endif
