#pragma once

#include "paracooba/common/timeout.h"
#include <memory>

namespace parac::communicator {
class Service;

class TimeoutController {
  public:
  TimeoutController(Service& service);
  ~TimeoutController();

  struct parac_timeout* setTimeout(uint64_t ms,
                                   void* userdata,
                                   parac_timeout_expired expiery_cb);

  void cancel(parac_timeout* timeout);

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;
  Service& m_service;

  void cancelIOThread(parac_timeout* timeout);
};
}
