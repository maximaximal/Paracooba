#include "paracooba/common/types.h"
#include "service.hpp"
#include <boost/filesystem/operations.hpp>
#include <paracooba/common/config.h>
#include <paracooba/common/log.h>
#include <paracooba/module.h>

#include <paracooba/communicator/communicator.h>

#include <cassert>

#include <parac_communicator_export.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#define COMMUNICATOR_NAME "cpp_asio_tcpconnections"
#define COMMUNICATOR_VERSION_MAJOR 1
#define COMMUNICATOR_VERSION_MINOR 0
#define COMMUNICATOR_VERSION_PATCH 0
#define COMMUNICATOR_VERSION_TWEAK 0

struct CommunicatorUserdata {
  CommunicatorUserdata(parac_handle& handle)
    : service(handle) {}

  parac::communicator::Service service;

  std::string temporary_directory;
  std::string default_tcp_listen_address;
  std::string default_udp_listen_address;
  std::string default_broadcast_address;

  parac_config_entry* config_entries;
};

static void
connect_to_remote(parac_module* mod, const char* remote) {
  assert(mod);
  assert(mod->userdata);
  CommunicatorUserdata* userdata =
    static_cast<CommunicatorUserdata*>(mod->userdata);
  userdata->service.connectToRemote(remote);
}

static parac_timeout*
set_timeout(parac_module* mod,
            uint64_t ms,
            void* userdata,
            parac_timeout_expired expiery_cb) {
  assert(mod);
  assert(mod->userdata);
  CommunicatorUserdata* comm =
    static_cast<CommunicatorUserdata*>(mod->userdata);
  return comm->service.setTimeout(ms, userdata, expiery_cb);
}

static void
connect_to_remote_inactive(parac_module* mod, const char* remote) {
  (void)mod;
  (void)remote;
}

static parac_timeout*
set_timeout_inactive(parac_module* mod,
                     uint64_t ms,
                     void* userdata,
                     parac_timeout_expired expiery_cb) {
  (void)mod;
  (void)ms;
  (void)userdata;
  (void)expiery_cb;
  return nullptr;
}

static void
init_config(CommunicatorUserdata* u) {
  using parac::communicator::Config;
  parac_config_entry* e = u->config_entries;

  parac_config_entry_set_str(
    &e[Config::TEMPORARY_DIRECTORY],
    "temporary-directory",
    "Directory to save temporary files (received formulas) in.");
  e[Config::TEMPORARY_DIRECTORY].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::TEMPORARY_DIRECTORY].type = PARAC_TYPE_STR;

  u->temporary_directory =
    (boost::filesystem::temp_directory_path() /
     ("/paracooba-tmp-" + std::to_string(u->service.handle().id)))
      .string();
  e[Config::TEMPORARY_DIRECTORY].default_value.string =
    u->temporary_directory.c_str();

  parac_config_entry_set_str(
    &e[Config::TCP_LISTEN_ADDRESS],
    "tcp-listen-address",
    "Address to listen on for incoming TCP connections.");
  e[Config::TCP_LISTEN_ADDRESS].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::TCP_LISTEN_ADDRESS].type = PARAC_TYPE_STR;

  u->default_tcp_listen_address = "0.0.0.0";
  e[Config::TCP_LISTEN_ADDRESS].default_value.string =
    u->default_tcp_listen_address.c_str();

  parac_config_entry_set_str(&e[Config::UDP_LISTEN_ADDRESS],
                             "udp-listen-address",
                             "Address to listen on for incoming UDP messages.");
  e[Config::UDP_LISTEN_ADDRESS].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::UDP_LISTEN_ADDRESS].type = PARAC_TYPE_STR;

  u->default_udp_listen_address =
    boost::asio::ip::address_v4::any().to_string();
  e[Config::UDP_LISTEN_ADDRESS].default_value.string =
    u->default_udp_listen_address.c_str();

  parac_config_entry_set_str(&e[Config::BROADCAST_ADDRESS],
                             "broadcast-address",
                             "Address to broadcast UDP messages to.");
  e[Config::BROADCAST_ADDRESS].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::BROADCAST_ADDRESS].type = PARAC_TYPE_STR;
  u->default_broadcast_address =
    boost::asio::ip::address_v4().broadcast().to_string();
  e[Config::BROADCAST_ADDRESS].default_value.string =
    u->default_broadcast_address.c_str();

  parac_config_entry_set_str(&e[Config::UDP_LISTEN_PORT],
                             "udp-listen-port",
                             "Port to listen on for UDP messages.");
  e[Config::UDP_LISTEN_PORT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::UDP_LISTEN_PORT].type = PARAC_TYPE_UINT16;
  e[Config::UDP_LISTEN_PORT].default_value.uint16 = 18001;

  parac_config_entry_set_str(&e[Config::UDP_TARGET_PORT],
                             "udp-target-port",
                             "Port to send UDP messages to.");
  e[Config::UDP_TARGET_PORT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::UDP_TARGET_PORT].type = PARAC_TYPE_UINT16;
  e[Config::UDP_TARGET_PORT].default_value.uint16 = 18001;

  parac_config_entry_set_str(&e[Config::TCP_LISTEN_PORT],
                             "tcp-listen-port",
                             "Port to listen on for TCP connections.");
  e[Config::TCP_LISTEN_PORT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::TCP_LISTEN_PORT].type = PARAC_TYPE_UINT16;
  e[Config::TCP_LISTEN_PORT].default_value.uint16 = 18001;

  parac_config_entry_set_str(&e[Config::TCP_TARGET_PORT],
                             "tcp-target-port",
                             "Port to target TCP connections to.");
  e[Config::TCP_TARGET_PORT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::TCP_TARGET_PORT].type = PARAC_TYPE_UINT16;
  e[Config::TCP_TARGET_PORT].default_value.uint16 = 18001;

  parac_config_entry_set_str(&e[Config::NETWORK_TIMEOUT],
                             "network-timeout",
                             "Timeout (in ms) for network connections.");
  e[Config::NETWORK_TIMEOUT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::NETWORK_TIMEOUT].type = PARAC_TYPE_UINT32;
  e[Config::NETWORK_TIMEOUT].default_value.uint32 = 5000;

  parac_config_entry_set_str(&e[Config::RETRY_TIMEOUT],
                             "retry-timeout",
                             "Timeout (in ms) for network connection retries.");
  e[Config::RETRY_TIMEOUT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::RETRY_TIMEOUT].type = PARAC_TYPE_UINT32;
  e[Config::RETRY_TIMEOUT].default_value.uint32 = 1000;

  parac_config_entry_set_str(
    &e[Config::MESSAGE_TIMEOUT],
    "message-timeout",
    "Timeout (in ms) for messages sent over the network. After this time, they "
    "are marked as failed. Messages can survive connection drops.");
  e[Config::MESSAGE_TIMEOUT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::MESSAGE_TIMEOUT].type = PARAC_TYPE_UINT16;
  e[Config::MESSAGE_TIMEOUT].default_value.uint16 = 10000u;

  parac_config_entry_set_str(
    &e[Config::CONNECTION_TIMEOUT],
    "connection-timeout",
    "Timeout (in ms) for connections to remote. After expiry, all offloaded "
    "tasks are returned to their queues and the work is re-scheduled.");
  e[Config::CONNECTION_TIMEOUT].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::CONNECTION_TIMEOUT].type = PARAC_TYPE_UINT32;
  e[Config::CONNECTION_TIMEOUT].default_value.uint32 = 30000u;

  parac_config_entry_set_str(&e[Config::KEEPALIVE_INTERVAL],
                             "keepalive-interval",
                             "Interval how often keepalive-packets should be "
                             "sent (at least, if no other messages were sent). "
                             "Must be smaller than network timeout of peers.");
  e[Config::KEEPALIVE_INTERVAL].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::KEEPALIVE_INTERVAL].type = PARAC_TYPE_UINT32;
  e[Config::KEEPALIVE_INTERVAL].default_value.uint32 = 1500;

  parac_config_entry_set_str(&e[Config::CONNECTION_RETRIES],
                             "connection-retries",
                             "Number of times making a connection to a remote "
                             "host should be retried after an error occurred.");
  e[Config::CONNECTION_RETRIES].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::CONNECTION_RETRIES].type = PARAC_TYPE_UINT32;
  e[Config::CONNECTION_RETRIES].default_value.uint32 = 30;

  parac_config_entry_set_str(&e[Config::KNOWN_REMOTES],
                             "known-remotes",
                             "Known remote hosts to connect to at program "
                             "startup. Deactivates UDP discovery.");
  e[Config::KNOWN_REMOTES].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::KNOWN_REMOTES].type = PARAC_TYPE_VECTOR_STR;

  parac_config_entry_set_str(
    &e[Config::AUTOMATIC_LISTEN_PORT_ASSIGNMENT],
    "disable-automatic-listen-port-assignment",
    "Disables automatic port trial and error process and stops the program if "
    "a supplied listen port is already bound.");
  e[Config::AUTOMATIC_LISTEN_PORT_ASSIGNMENT].registrar =
    PARAC_MOD_COMMUNICATOR;
  e[Config::AUTOMATIC_LISTEN_PORT_ASSIGNMENT].type = PARAC_TYPE_SWITCH;

  parac_config_entry_set_str(
    &e[Config::ENABLE_UDP],
    "enable-udp",
    "Enables listening for incoming UDP announcements");
  e[Config::ENABLE_UDP].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::ENABLE_UDP].type = PARAC_TYPE_SWITCH;

  parac_config_entry_set_str(
    &e[Config::UDP_ANNOUNCEMENT_INTERVAL_MS],
    "udp-announcement-interval",
    "Interval for UDP online announcements in milliseconds. Automatically "
    "discovers other nodes which have --enable-udp. 0 means no announcements "
    "from this node. Automatically activates this node's --enable-udp.");
  e[Config::UDP_ANNOUNCEMENT_INTERVAL_MS].registrar = PARAC_MOD_COMMUNICATOR;
  e[Config::UDP_ANNOUNCEMENT_INTERVAL_MS].type = PARAC_TYPE_UINT32;
  e[Config::UDP_ANNOUNCEMENT_INTERVAL_MS].default_value.uint32 = 0;
}

static bool
is_tcpacceptor_active(parac_module* mod) {
  assert(mod);
  assert(mod->userdata);

  CommunicatorUserdata* userdata =
    static_cast<CommunicatorUserdata*>(mod->userdata);

  return userdata->service.isTCPAcceptorActive();
}

static parac_status
pre_init(parac_module* mod) {
  assert(mod);
  assert(mod->communicator);
  assert(mod->handle);
  assert(mod->handle->config);

  mod->communicator->tcp_acceptor_active = is_tcpacceptor_active;
  mod->communicator->connect_to_remote = &connect_to_remote;
  mod->communicator->set_timeout = &set_timeout;

  CommunicatorUserdata* userdata =
    static_cast<CommunicatorUserdata*>(mod->userdata);

  assert(userdata->config_entries);

  userdata->service.applyConfig(userdata->config_entries);

  return PARAC_OK;
}

static parac_status
init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);
  assert(mod->handle->thread_registry);

  CommunicatorUserdata* userdata =
    static_cast<CommunicatorUserdata*>(mod->userdata);
  return userdata->service.start();
}

static parac_status
mod_request_exit(parac_module* mod) {
  assert(mod);
  assert(mod->communicator);
  assert(mod->handle);
  assert(mod->userdata);

  mod->communicator->set_timeout = &set_timeout_inactive;
  mod->communicator->connect_to_remote = &connect_to_remote_inactive;

  CommunicatorUserdata* userdata =
    static_cast<CommunicatorUserdata*>(mod->userdata);
  userdata->service.requestStop();

  return PARAC_OK;
}

static parac_status
mod_exit(parac_module* mod) {
  assert(mod);
  assert(mod->communicator);
  assert(mod->handle);

  CommunicatorUserdata* userdata =
    static_cast<CommunicatorUserdata*>(mod->userdata);
  if(userdata) {
    delete userdata;
    mod->userdata = nullptr;
  }

  return PARAC_OK;
}

extern "C" PARAC_COMMUNICATOR_EXPORT parac_status
parac_module_discover_communicator(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_COMMUNICATOR);
  assert(mod);

  mod->type = PARAC_MOD_COMMUNICATOR;
  mod->name = COMMUNICATOR_NAME;
  mod->version.major = COMMUNICATOR_VERSION_MAJOR;
  mod->version.minor = COMMUNICATOR_VERSION_MINOR;
  mod->version.patch = COMMUNICATOR_VERSION_PATCH;
  mod->version.tweak = COMMUNICATOR_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;
  mod->request_exit = mod_request_exit;
  mod->exit = mod_exit;

  using parac::communicator::Config;

  CommunicatorUserdata* userdata = new CommunicatorUserdata(*handle);
  mod->userdata = static_cast<void*>(userdata);

  userdata->config_entries =
    parac_config_reserve(handle->config, static_cast<size_t>(Config::_COUNT));
  assert(userdata->config_entries);

  init_config(userdata);

  return PARAC_OK;
}
