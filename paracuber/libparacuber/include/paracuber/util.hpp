#ifndef PARACUBER_UTIL_HPP
#define PARACUBER_UTIL_HPP

#include <cstdint>
#include <shared_mutex>
#include <string>

namespace paracuber {
// This is inspired from https://stackoverflow.com/a/2074403
template<typename T>
inline T
FastAbsolute(T v)
{
  T temp = v >> ((sizeof(T) * 8) - 1); /* make a mask of the sign bit */
  v ^= temp;     /* toggle the bits if value is negative */
  v += temp & 1; /* add one if value was negative */
  return v;
}

std::string
BytePrettyPrint(size_t bytes);

template<typename T>
using SharedLockView = std::pair<T, std::shared_lock<std::shared_mutex>>;
template<typename T>
using ConstSharedLockView =
  std::pair<const T&, std::shared_lock<std::shared_mutex>>;

extern int64_t StackStart;

size_t
GetStackSize();
}

#endif
