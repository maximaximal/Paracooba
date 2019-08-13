#ifndef PARACUBER_CONFIG_HPP

#include <any>
#include <array>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstddef>
#include <string_view>

namespace paracuber {
/** @brief Store for communication options specific for each node.
 *
 */
class Config
{
  public:
  /** Configuration variable differentiator enumeration.
   */
  enum Key
  {
    LocalName,
    InputFile,
    ThreadCount,
    UDPTargetPort,
    UDPListenPort,
    Id,
    DaemonHost,
    WorkQueueCapacity,

    _KEY_COUNT
  };

  /** @brief Constructor
   */
  Config();
  /** @brief Destructor.
   */
  ~Config();

  /** @brief Parse command line parameters and also process provided
   * configuration files.
   *
   * @return True if program execution may continue, false if program should be
   * terminated.
   */
  bool parseParameters(int argc, char* argv[]);
  /** @brief Parse command line parameters and also process provided
   * configuration files.
   *
   * @return True if program execution may continue, false if program should be
   * terminated.
   */
  bool parseConfigFile(std::string_view filePath);

  /** @brief Get a configuration variable with type and key.
   */
  template<typename T>
  inline T& get(Key key)
  {
    return std::any_cast<T&>(m_config[key]);
  }
  /** @brief Get a std::string configuration variable.
   */
  inline std::string_view getString(Key key)
  {
    std::string& str = get<std::string>(key);
    return std::string_view{ str.c_str(), str.size() };
  }
  /** @brief Get an int32 configuration variable.
   */
  inline int32_t getInt32(Key key) { return get<int32_t>(key); }
  /** @brief Get an int64 configuration variable.
   */
  inline int64_t getInt64(Key key) { return get<int64_t>(key); }
  /** @brief Get an uint16 configuration variable.
   */
  inline uint32_t getUint16(Key key) { return get<uint16_t>(key); }
  /** @brief Get an uint32 configuration variable.
   */
  inline uint32_t getUint32(Key key) { return get<uint32_t>(key); }
  /** @brief Get an uint64 configuration variable.
   */
  inline uint64_t getUint64(Key key) { return get<uint64_t>(key); }
  /** @brief Get a bool configuration variable.
   */
  inline bool getBool(Key key) { return get<bool>(key); }

  /** @brief Get a configuration variable which can be cast in any way.
   */
  inline std::any get(Key key) { return m_config[key]; }
  /** @brief Set a configuration variable.
   */
  inline void set(Key key, std::any val) { m_config[key] = val; }

  /** @brief Get a configuration variable which can be cast in any way.
   */
  std::any operator[](Key key) { return get(key); }

  /** @brief Check if debug mode is active. */
  inline bool isDebugMode() { return m_debugMode; }
  /** @brief Check if info mode is active. */
  inline bool isInfoMode() { return m_infoMode; }
  /** @brief Check if daemon mode is active. */
  inline bool isDaemonMode() { return m_daemonMode; }

  /** @brief Set debug mode active. */
  inline void setDebugMode(bool v) { m_debugMode = v; }
  /** @brief Set info mode active. */
  inline void setInfoMode(bool v) { m_infoMode = v; }
  /** @brief Set daemon mode active. */
  inline void setDaemonMode(bool v) { m_daemonMode = v; }

  private:
  bool processCommonParameters(
    const boost::program_options::variables_map& map);

  using ConfigArray =
    std::array<std::any, static_cast<std::size_t>(_KEY_COUNT)>;
  ConfigArray m_config;

  boost::program_options::options_description m_optionsCLI;
  boost::program_options::options_description m_optionsCommon;
  boost::program_options::options_description m_optionsFile;

  bool m_debugMode = false;
  bool m_infoMode = false;
  bool m_daemonMode = false;
};

constexpr const char*
GetConfigNameFromEnum(Config::Key key)
{
  switch(key) {
    case Config::LocalName:
      return "local-name";
    case Config::InputFile:
      return "input-file";
    case Config::ThreadCount:
      return "threads";
    case Config::UDPListenPort:
      return "udp-listen-port";
    case Config::UDPTargetPort:
      return "udp-target-port";
    case Config::Id:
      return "id";
    case Config::DaemonHost:
      return "daemon-host";
    case Config::WorkQueueCapacity:
      return "work-queue-capacity";
    default:
      return "";
  }
}
}

#endif
