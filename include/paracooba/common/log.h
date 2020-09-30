#ifndef PARACOOBA_COMMON_PATH_H
#define PARACOOBA_COMMON_PATH_H

#include "paracooba/common/status.h"
#include "paracooba/common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum parac_log_severity {
  PARAC_TRACE,
  PARAC_DEBUG,
  PARAC_INFO,
  PARAC_LOCALWARNING,
  PARAC_LOCALERROR,
  PARAC_GLOBALWARNING,
  PARAC_GLOBALERROR,
  PARAC_FATAL,
  PARAC_SEVERITY_COUNT
};

enum parac_log_channel {
  PARAC_GENERAL,
  PARAC_COMMUNICATOR,
  PARAC_BROKER,
  PARAC_RUNNER,
  PARAC_SOLVER,
  PARAC_CUBER,
  PARAC_LOADER,
  PARAC_CHANNEL_COUNT
};

parac_status
parac_log_init(struct parac_thread_registry* thread_registry);

void
parac_log_set_severity(parac_log_severity severity);

void
parac_log_set_channel_active(parac_log_channel channel, bool active);

void
parac_log_set_local_id(parac_id id);

void
parac_log_set_local_name(const char* name);

const char*
parac_log_severity_to_str(parac_log_severity severity);

const char*
parac_log_channel_to_str(parac_log_channel channel);

void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          const char* msg);

#ifdef __cplusplus
}
#include <ostream>
#include <string_view>

void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          std::string_view msg);

inline std::ostream&
operator<<(std::ostream& o, parac_log_severity severity) {
  return o << parac_log_severity_to_str(severity);
}
inline std::ostream&
operator<<(std::ostream& o, parac_log_channel channel) {
  return o << parac_log_channel_to_str(channel);
}

#ifdef PARAC_LOG_INCLUDE_FMT
#include <fmt/format.h>
#include <fmt/ostream.h>

template<typename FormatString,
         typename std::enable_if<fmt::is_compile_string<FormatString>::value,
                                 int>::type = 0,
         typename... Args>
void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          const FormatString& fmt,
          const Args&... args) {
  parac_log(channel, severity, fmt, args...);
}

template<typename... Args>
void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          fmt::string_view fmt,
          const Args&... args) {
  parac_log(channel, severity, fmt, args...);
}
template<typename FormatString, typename... Args>
void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          const FormatString& fmt,
          const Args&... args) {
  try {
    fmt::memory_buffer buf;
    fmt::format_to(buf, fmt, args...);
    parac_log(channel, severity, std::string_view(buf.data(), buf.size()));
  } catch(...) {
  }
}
#endif
#endif

#endif
