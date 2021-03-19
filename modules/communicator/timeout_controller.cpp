#include "timeout_controller.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/timeout.h"
#include "service.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/pool/poolfwd.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <chrono>
#include <list>
#include <mutex>

#include <boost/pool/pool_alloc.hpp>

namespace parac::communicator {
struct TimeoutController::Internal {
  std::mutex timeoutMutex;

  struct Timeout;
  struct Task;
  template<typename T>
  using Allocator =
    boost::fast_pool_allocator<T,
                               boost::default_user_allocator_new_delete,
                               boost::details::pool::default_mutex,
                               64>;

  using TimeoutList = std::list<Timeout, Allocator<Timeout>>;

  using TimerPool = boost::singleton_pool<boost::asio::steady_timer,
                                          sizeof(boost::asio::steady_timer)>;

  struct Timeout {
    TimeoutList::iterator it;
    parac_timeout timeout;
    boost::asio::steady_timer* timer;
    boost::pool<>* pool;

    // Also deletes the current timeout!
    void cancel() { timer->cancel(); }

    Timeout(Service& service) {
      void* mem = TimerPool::malloc();
      timer = new(mem) boost::asio::steady_timer(service.ioContext());
    }
    ~Timeout() {
      if(timeout.expired) {
        timeout.expired(&timeout);
        timeout.expired = nullptr;
      }
      TimerPool::free(timer);
    }
  };

  TimeoutList timeoutList;
};

TimeoutController::TimeoutController(Service& service)
  : m_internal(std::make_unique<Internal>())
  , m_service(service) {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Create TimeoutController");

  static_assert(
    std::is_standard_layout_v<TimeoutController::Internal::Timeout>);
}
TimeoutController::~TimeoutController() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Destroy TimeoutController");
  std::unique_lock lock(m_internal->timeoutMutex);
  m_internal->timeoutList.clear();
}

parac_timeout*
TimeoutController::setTimeout(uint64_t ms,
                              void* userdata,
                              parac_timeout_expired expiery_cb) {
  Internal::Timeout* elem = nullptr;
  {
    std::unique_lock lock(m_internal->timeoutMutex);
    elem = &m_internal->timeoutList.emplace_front(m_service);
    elem->it = m_internal->timeoutList.begin();
  }
  elem->timeout.cancel_userdata = this;
  elem->timeout.expired_userdata = userdata;
  elem->timeout.expired = expiery_cb;
  elem->timeout.cancel = [](parac_timeout* t) {
    assert(t);
    assert(t->cancel_userdata);

    // Timer can no longer expire when it is canceled.
    t->expired_userdata = nullptr;
    t->expired = nullptr;

    TimeoutController* c = static_cast<TimeoutController*>(t->cancel_userdata);
    c->cancel(t);
  };
  elem->timer->expires_from_now(std::chrono::milliseconds(ms));
  elem->timer->async_wait([this, elem](const boost::system::error_code& errc) {
    if(!errc) {
      if(elem->timeout.expired) {
        elem->timeout.expired(&elem->timeout);
        cancel(&elem->timeout);
      }
    }
  });
  return &elem->timeout;
}
void
TimeoutController::cancel(parac_timeout* timeout) {
  assert(timeout);
  assert(timeout->cancel_userdata);

  timeout->expired = nullptr;

  Internal::Timeout* t =
    reinterpret_cast<Internal::Timeout*>(reinterpret_cast<std::byte*>(timeout) -
                                         offsetof(Internal::Timeout, timeout));
  assert(t);

  std::unique_lock lock(m_internal->timeoutMutex);
  m_internal->timeoutList.erase(t->it);
}
}
