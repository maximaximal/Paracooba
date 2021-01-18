#pragma once

#include <atomic>

namespace parac {
class SpinLock {
  public:
  explicit SpinLock(std::atomic_flag& f)
    : f(f) {
    while(f.test_and_set()) {
      // WAITING
    }
  }
  ~SpinLock() noexcept { f.clear(); }

  private:
  std::atomic_flag& f;
};
}
