#ifndef PARACUBER_MESSAGES_DAEMON
#define PARACUBER_MESSAGES_DAEMON

#include <cstdint>
#include <forward_list>
#include <string>

#include <cereal/access.hpp>
#include <cereal/types/forward_list.hpp>

#include "daemon_context.hpp"

namespace paracuber {
namespace messages {
class Daemon
{
  public:
  Daemon() {}
  ~Daemon() {}

  using ContextList = std::forward_list<DaemonContext>;

  ContextList& getContexts() { return contexts; }
  const ContextList& getContexts() const { return contexts; }

  void addContext(const DaemonContext& ctx) { contexts.push_front(ctx); }
  void addContext(const DaemonContext&& ctx)
  {
    contexts.push_front(std::move(ctx));
  }

  private:
  friend class cereal::access;

  ContextList contexts;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(contexts);
  }
};
}
}

#endif
