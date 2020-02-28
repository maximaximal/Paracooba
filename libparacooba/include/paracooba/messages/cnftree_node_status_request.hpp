#ifndef PARACOOBA_MESSAGES_CNFTREE_NODE_STATUS_REQUEST
#define PARACOOBA_MESSAGES_CNFTREE_NODE_STATUS_REQUEST

#include <cstdint>
#include <optional>
#include <stack>

#include <cereal/access.hpp>
#include <cereal/types/stack.hpp>

namespace paracooba {
namespace messages {
class CNFTreeNodeStatusRequest
{
  public:
  using HandleStack = std::stack<int64_t>;

  CNFTreeNodeStatusRequest() {}
  CNFTreeNodeStatusRequest(int64_t handle, uint64_t path, int64_t cnfId)

    : path(path)
    , cnfId(cnfId)
  {
    handleStack.push(handle);
  }
  CNFTreeNodeStatusRequest(int64_t handle,
                           int64_t localId,
                           uint64_t path,
                           int64_t cnfId)

    : path(path)
    , cnfId(cnfId)
  {
    handleStack.push(handle);
    handleStack.push(localId);
  }
  CNFTreeNodeStatusRequest(int64_t localId,
                           const CNFTreeNodeStatusRequest& request)
    : path(request.getPath())
    , cnfId(request.cnfId)
    , handleStack(request.getHandleStack())
  {
    handleStack.push(localId);
  }
  virtual ~CNFTreeNodeStatusRequest() {}

  int64_t getHandle() const { return handleStack.top(); }
  uint64_t getPath() const { return path; }
  int64_t getCnfId() const { return cnfId; }

  const HandleStack& getHandleStack() const { return handleStack; }

  private:
  friend class cereal::access;

  uint64_t path;
  int64_t cnfId;
  HandleStack handleStack;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(path), CEREAL_NVP(cnfId), CEREAL_NVP(handleStack));
  }
};
}
}

#endif
