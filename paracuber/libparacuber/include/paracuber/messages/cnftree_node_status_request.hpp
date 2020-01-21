#ifndef PARACUBER_MESSAGES_CNFTREE_NODE_STATUS_REQUEST
#define PARACUBER_MESSAGES_CNFTREE_NODE_STATUS_REQUEST

#include <cstdint>
#include <optional>

#include <cereal/access.hpp>

namespace paracuber {
namespace messages {
class CNFTreeNodeStatusRequest
{
  public:
  CNFTreeNodeStatusRequest() {}
  CNFTreeNodeStatusRequest(int64_t handle, uint64_t path, int64_t cnfId)
    : handle(handle)
    , path(path)
    , cnfId(cnfId)
  {}
  virtual ~CNFTreeNodeStatusRequest() {}

  int64_t getHandle() const { return handle; }
  uint64_t getPath() const { return path; }
  int64_t getCnfId() const { return cnfId; }

  private:
  friend class cereal::access;

  int64_t handle;
  uint64_t path;
  int64_t cnfId;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(handle), CEREAL_NVP(path), CEREAL_NVP(cnfId));
  }
};
}
}

#endif
