#ifndef PARACUBER_UTIL_HPP
#define PARACUBER_UTIL_HPP

#include <cstdint>
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

}

#endif
