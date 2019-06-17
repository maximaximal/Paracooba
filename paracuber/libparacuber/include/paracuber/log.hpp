#ifndef PARACUBER_LOG_HPP
#define PARACUBER_LOG_HPP

#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/manipulators/to_log.hpp>
#include <memory>
#include <string>

#ifndef FILE_BASENAME
#define FILE_BASENAME "Basename not supported."
#endif

#define PARACUBER_LOG_LOCATION(lg)                                           \
  boost::log::attribute_cast<boost::log::attributes::mutable_constant<int>>( \
    boost::log::core::get()->get_global_attributes()["Line"])                \
    .set(__LINE__);                                                          \
  boost::log::attribute_cast<                                                \
    boost::log::attributes::mutable_constant<std::string>>(                  \
    boost::log::core::get()->get_global_attributes()["File"])                \
    .set(FILE_BASENAME);                                                     \
  boost::log::attribute_cast<                                                \
    boost::log::attributes::mutable_constant<std::string>>(                  \
    boost::log::core::get()->get_global_attributes()["Function"])            \
    .set(__PRETTY_FUNCTION__);

#define PARACUBER_LOG(LOGGER, SEVERITY) \
  do {                                  \
    PARACUBER_LOG_LOCATION(LOGGER)      \
  } while(false);                       \
  BOOST_LOG_SEV(LOGGER, ::paracuber::Log::Severity::SEVERITY)

namespace paracuber {
class Config;

/** @brief Utility class to manage logging.
 */
class Log
{
  public:
  /** @brief Tag to associate severity internally.
   */
  struct Severity_Tag;
  /** @brief Severity of a log message.
   *
   * Local errors should be differentiated from global errors! Local errors only
   * affect local node, a global error may affect the entire distributed solving
   * process over multiple nodes.
   */
  enum Severity
  {
    Trace,
    Debug,
    Info,
    LocalWarning,
    LocalError,
    GlobalWarning,
    GlobalError,
    Fatal
  };

  /** @brief Constructor
   */
  Log(std::shared_ptr<Config> config);
  /** @brief Destructor.
   */
  ~Log();

  /** @brief Create a logger for a specific environment, which may receive
   * multiple custom attributes.
   */
  boost::log::sources::severity_logger<Severity> createLogger();

  private:
  std::shared_ptr<Config> m_config;
  boost::shared_ptr<boost::log::sinks::synchronous_sink<
    boost::log::sinks::basic_text_ostream_backend<char>>>
    m_consoleSink;
};

std::ostream&
operator<<(std::ostream& strm, ::paracuber::Log::Severity level);

boost::log::formatting_ostream&
operator<<(
  boost::log::formatting_ostream& strm,
  boost::log::to_log_manip<::paracuber::Log::Severity,
                           ::paracuber::Log::Severity_Tag> const& manip);
}

#endif
