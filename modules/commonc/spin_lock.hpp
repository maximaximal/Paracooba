#pragma once

#include <atomic>

namespace parac {
class SpinLock {
  public:
  SpinLock() = delete;
  SpinLock(const SpinLock&) = delete;
  SpinLock(SpinLock&& o)
    : f(o.f) {
    o.f = nullptr;
  }
  void operator=(const SpinLock&) = delete;

  explicit SpinLock(std::atomic_flag& f)
    : f(&f) {
    while(f.test_and_set()) {
      // WAITING
    }
  }
  ~SpinLock() noexcept {
    if(f)
      f->clear();
  }

  private:
  std::atomic_flag* f;
};
}
