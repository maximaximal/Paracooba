#ifndef PARACOOBA_UTIL_HPP
#define PARACOOBA_UTIL_HPP

#include <cstdint>
#include <shared_mutex>
#include <string>

namespace paracooba {
class CNF;
class Config;

/** @brief Fast calculation of the absolute value of a number using
 * bit-fiddling.
 *
 * This is inspired from https://stackoverflow.com/a/2074403
 */
template<typename T>
inline T
FastAbsolute(T v)
{
  T temp = v >> ((sizeof(T) * 8) - 1); /* make a mask of the sign bit */
  v ^= temp;     /* toggle the bits if value is negative */
  v += temp & 1; /* add one if value was negative */
  return v;
}

/** @brief Pretty-print bytes into KiB, MiB, GiB, ...
 */
std::string
BytePrettyPrint(size_t bytes);

template<typename T>
using UniqueLockView = std::pair<T, std::unique_lock<std::shared_mutex>>;
template<typename T>
using SharedLockView = std::pair<T, std::shared_lock<std::shared_mutex>>;
template<typename T>
using ConstSharedLockView =
  std::pair<const T&, std::shared_lock<std::shared_mutex>>;

/** @brief Global variable for start of stack (beginning of main()).
 */
extern int64_t StackStart;

/** @brief Get the current stack size in bytes.
 */
size_t
GetStackSize();

/** @brief Get the root CNF formula, mode-agnostic (Client or Daemon).
 */
std::shared_ptr<CNF>
GetRootCNF(Config* config, int64_t cnfID);

/** @brief Cast a unique_ptr to another unique_ptr in the same way static_cast
 * is doing.
 *
 * Approach to safely cast unique_ptr from
 * https://stackoverflow.com/a/36120483
 */
template<typename TO, typename FROM>
std::unique_ptr<TO>
static_unique_pointer_cast(std::unique_ptr<FROM>&& old)
{
  return std::unique_ptr<TO>{ static_cast<TO*>(old.release()) };
}
}

#endif
