#pragma once

#include <cstdio>
#include <memory>

namespace parac::util {
struct FileDeleter {
  void operator()(std::FILE* fp) { std::fclose(fp); }
};

using UniqueFile = std::unique_ptr<std::FILE, FileDeleter>;
}
