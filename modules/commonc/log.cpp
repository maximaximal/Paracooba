#include "paracooba/common/status.h"
#include <boost/log/expressions/message.hpp>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <cassert>
#include <iostream>
#include <parac_common_export.h>
#include <string_view>

using LoggerMT =
  boost::log::sources::severity_channel_logger_mt<parac_log_severity,
                                                  parac_log_channel>;

static boost::shared_ptr<boost::log::sinks::synchronous_sink<
  boost::log::sinks::basic_text_ostream_backend<char>>>
  global_console_sink;

static LoggerMT global_logger(boost::log::keywords::channel = PARAC_GENERAL);

static parac_log_severity global_severity = PARAC_INFO;
static std::array<bool, PARAC_CHANNEL_COUNT> global_channels;

static std::string local_name = "Unnamed";
static parac_id local_id = 0;

using namespace boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(parac_logger_timestamp,
                            "TimeStamp",
                            boost::posix_time::ptime)

static void
log_new_thread_callback(parac_thread_registry_handle* handle) {
  assert(handle);
  boost::log::core::get()->add_thread_attribute(
    "ThreadID", boost::log::attributes::constant<uint16_t>(handle->thread_id));
}

PARAC_COMMON_EXPORT parac_status
parac_log_init(parac_thread_registry* thread_registry) {
  try {
    global_channels.fill(true);

    add_common_attributes();
    boost::log::core::get()->add_thread_attribute(
      "ThreadID", boost::log::attributes::constant<uint16_t>(0));

    parac_thread_registry_add_starting_callback(thread_registry,
                                                &log_new_thread_callback);

    global_console_sink = add_console_log(std::clog);
    global_console_sink->set_formatter(
      expressions::stream << "c ["
                          << expressions::attr<std::string>("LocalName") << "|"
                          << expressions::attr<parac_id>("LocalID") << "] ["
                          << parac_logger_timestamp << "] ["
                          << expressions::attr<parac_log_severity>("Severity")
                          << "] ["
                          << expressions::attr<parac_log_channel>("Channel")
                          << " @ T" << expressions::attr<uint16_t>("ThreadID")
                          << "] " << expressions::smessage);
    boost::log::core::get()->add_sink(global_console_sink);
  } catch(std::exception& e) {
    std::cerr << "> Exception during log setup! Message: " << e.what()
              << std::endl;
    return PARAC_GENERIC_ERROR;
  }
  return PARAC_OK;
}

PARAC_COMMON_EXPORT void
parac_log_set_severity(parac_log_severity severity) {
  global_severity = severity;
}

PARAC_COMMON_EXPORT void
parac_log_set_channel_active(parac_log_channel channel, bool active) {
  global_channels[channel] = active;
}

PARAC_COMMON_EXPORT void
parac_log_set_local_id(parac_id id) {
  local_id = id;
  global_logger.add_attribute("LocalID", attributes::make_constant(local_id));
}

PARAC_COMMON_EXPORT void
parac_log_set_local_name(const char* name) {
  local_name = name;
  global_logger.add_attribute("LocalName",
                              attributes::make_constant(local_name));
}

PARAC_COMMON_EXPORT void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          const char* msg) {
  parac_log(channel, severity, std::string_view(msg));
}

PARAC_COMMON_EXPORT void
parac_log(parac_log_channel channel,
          parac_log_severity severity,
          std::string_view msg) {
  if(severity >= global_severity && global_channels[channel]) {
    try {
      BOOST_LOG_CHANNEL_SEV(global_logger, channel, severity) << msg;
    } catch(std::exception& e) {
      std::cerr
        << "!! Could not print log entry because of exception! Message: "
        << e.what() << std::endl;
    }
  }
}

PARAC_COMMON_EXPORT const char*
parac_log_severity_to_str(parac_log_severity severity) {
  switch(severity) {
    case PARAC_TRACE:
      return "TRCE";
    case PARAC_DEBUG:
      return "DEBG";
    case PARAC_INFO:
      return "INFO";
    case PARAC_LOCALWARNING:
      return "LWRN";
    case PARAC_LOCALERROR:
      return "LERR";
    case PARAC_GLOBALWARNING:
      return "GWRN";
    case PARAC_GLOBALERROR:
      return "GERR";
    case PARAC_FATAL:
      return "FTAL";

    case PARAC_SEVERITY_COUNT:
      break;
  }
  return "!!!!";
}

PARAC_COMMON_EXPORT const char*
parac_log_channel_to_str(parac_log_channel source) {
  switch(source) {
    case PARAC_GENERAL:
      return "General";
    case PARAC_COMMUNICATOR:
      return "Communicator";
    case PARAC_BROKER:
      return "Broker";
    case PARAC_RUNNER:
      return "Runner";
    case PARAC_SOLVER:
      return "Solver";
    case PARAC_CUBER:
      return "Cuber";
    case PARAC_LOADER:
      return "Loader";
    case PARAC_CHANNEL_COUNT:
      break;
  }
  return "!";
}
