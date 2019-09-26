#include "../include/paracuber/util.hpp"
#include <cmath>

namespace paracuber {
std::string
BytePrettyPrint(size_t bytes)
{
  auto base = (double)std::log(bytes) / (double)std::log(1024);
  const char* suffixArr[] = { " ", "kiB", "MiB", "GiB", "TiB", "PiB" };
  return std::to_string(
           (size_t)std::round(std::pow(1024, base - std::floor(base)))) +
         suffixArr[(size_t)std::floor(base)];
}
int64_t StackStart = 0;

size_t
GetStackSize()
{
  char c = 'T';
  return StackStart - reinterpret_cast<int64_t>(&c);
}
}
