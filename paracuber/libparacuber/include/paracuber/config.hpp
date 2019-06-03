#ifndef PARACUBER_CONFIG_HPP

#include <any>
#include <array>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstddef>
#include <string_view>

namespace paracuber {
class Config
{
  public:
  enum Key
  {
    LocalName,

    _KEY_COUNT
  };

  Config();
  ~Config();

  bool parseParameters(int argc, char* argv[]);
  bool parseConfigFile(std::string_view filePath);

  template<typename T>
  inline T& get(Key key)
  {
    return std::any_cast<T&>(m_config[key]);
  }
  inline std::string_view getString(Key key)
  {
    std::string& str = get<std::string>(key);
    return std::string_view{ str.c_str(), str.size() };
  }
  inline int64_t getInt64(Key key) { return get<int64_t>(key); }
  inline int32_t getInt32(Key key) { return get<int32_t>(key); }

  inline std::any get(Key key) { return m_config[key]; }
  inline void set(Key key, std::any val) { m_config[key] = val; }

  std::any operator[](Key key) { return get(key); }

  private:
  bool processCommonParameters(
    const boost::program_options::variables_map& map);

  using ConfigArray =
    std::array<std::any, static_cast<std::size_t>(_KEY_COUNT)>;
  ConfigArray m_config;

  boost::program_options::options_description m_optionsCLI;
  boost::program_options::options_description m_optionsCommon;
  boost::program_options::options_description m_optionsFile;
};

constexpr const char*
GetConfigNameFromEnum(Config::Key key)
{
  switch(key) {
    case Config::LocalName:
      return "local-name";
    default:
      return "";
  }
}
}

#endif
