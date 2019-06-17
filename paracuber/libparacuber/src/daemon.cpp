#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/log.hpp"

namespace paracuber {
Daemon::Daemon(std::shared_ptr<Config> config)
  : m_config(config)
{}

Daemon::~Daemon() {}
}
