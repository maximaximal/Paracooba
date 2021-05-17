#pragma once

#include <fstream>
#include <string_view>

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

namespace parac {
/** @brief Utility function to write a string_view into a temporary file.
 * Used e.g. to provide inline string inputs during testing.
 */
inline std::pair<parac_status, std::string>
StringToFile(std::string_view str) {
  auto h = std::hash<std::string_view>{}(str);
  std::string path =
    PARAC_DEFAULT_TEMP_PATH "/paracooba-tmp-file-" + std::to_string(h);

  {
    std::ofstream out(path);
    out << str;
    out.flush();
  }

  return { PARAC_OK, path };
}
}
