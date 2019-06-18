#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/log.hpp"
#include "../include/paracuber/communicator.hpp"

namespace paracuber {
Daemon::Daemon(ConfigPtr config,
               LogPtr log,
               std::shared_ptr<Communicator> communicator)
  : m_config(config)
{}

Daemon::~Daemon() {}
}
