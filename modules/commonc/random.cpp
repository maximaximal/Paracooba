#include <chrono>
#include <random>
#include <thread>

#include "paracooba/common/random.h"

#include <parac_common_export.h>
#include <thread>

static inline std::mt19937&
random_mt() {
  // Try to generate as-random-as-possible initial seed value based on time and
  // thread ID.

  static thread_local std::mt19937 mt(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch())
      .count() +
    std::hash<std::thread::id>()(std::this_thread::get_id()));
  return mt;
}

template<typename T>
static inline T
rand_normal_distribution(T mean, double s) {
  std::normal_distribution<> distr(mean, s);
  return distr(random_mt());
}

template<typename T>
static inline T
rand_uniform_distribution(T min, T max) {
  std::uniform_int_distribution<T> distr(min, max);
  return distr(random_mt());
}

PARAC_COMMON_EXPORT uint32_t
parac_uint32_normal_distribution(uint32_t mean, double s) {
  return rand_normal_distribution(mean, s);
}

PARAC_COMMON_EXPORT int64_t
parac_int64_normal_distribution(int64_t mean, double s) {
  return rand_normal_distribution(mean, s);
}

PARAC_COMMON_EXPORT size_t
parac_size_normal_distribution(size_t mean, double s) {
  return rand_normal_distribution(mean, s);
}

PARAC_COMMON_EXPORT uint32_t
parac_uint32_uniform_distribution(uint32_t min, uint32_t max) {
  return rand_uniform_distribution(min, max);
}

PARAC_COMMON_EXPORT int64_t
parac_int64_uniform_distribution(int64_t min, int64_t max) {
  return rand_uniform_distribution(min, max);
}

PARAC_COMMON_EXPORT size_t
parac_size_uniform_distribution(size_t min, size_t max) {
  return rand_uniform_distribution(min, max);
}
