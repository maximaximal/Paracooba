#ifndef PARACOOBA_MESSAGES_NEW_REMOTE
#define PARACOOBA_MESSAGES_NEW_REMOTE

#include <string>

#include "../types.hpp"

namespace paracooba {
namespace messages {

/** @brief Announces a new node in the cluster.
 *
 * This should only be sent from the Master node to daemons. This way, a fully
 * interconnected cluster can be built using only a reference to the master
 * node.
 */
class NewRemoteConnected
{
  public:
  NewRemoteConnected() {}
  NewRemoteConnected(const std::string& remote, ID id)
    : remote(remote)
    , id(id)
  {}
  virtual ~NewRemoteConnected() {}

  const std::string& getRemote() const { return remote; }
  ID getId() const { return id; }

  private:
  friend class cereal::access;

  std::string remote;
  ID id;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(remote), CEREAL_NVP(id));
  }
};
}
}

#endif
