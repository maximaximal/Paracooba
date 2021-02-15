#include "paracooba/common/compute_node_store.h"
#include "paracooba/common/config.h"
#include "paracooba/common/task_store.h"
#include "paracooba/common/types.h"
#include <paracooba/broker/broker.h>
#include <paracooba/module.h>

#include "paracooba/common/log.h"

#include <cassert>

#include <parac_broker_export.h>

#include "broker_compute_node_store.hpp"
#include "broker_task_store.hpp"

#define BROKER_NAME "cpp_nodemap_keepstocked"
#define BROKER_VERSION_MAJOR 1
#define BROKER_VERSION_MINOR 0
#define BROKER_VERSION_PATCH 0
#define BROKER_VERSION_TWEAK 0

struct BrokerUserdata {
  void pre_init(parac_handle& handle,
                uint32_t autoShutdownTimeout,
                bool autoShutdownAfterFirstFinishedClient) {
    internal = std::make_unique<Internal>(
      handle, autoShutdownTimeout, autoShutdownAfterFirstFinishedClient);
  }

  struct Internal {
    Internal(parac_handle& handle,
             uint32_t autoShutdownTimeout,
             bool autoShutdownAfterFirstFinishedClient)
      : taskStore(handle, task_store_interface, autoShutdownTimeout)
      , computeNodeStore(handle,
                         compute_node_store_interface,
                         taskStore,
                         autoShutdownAfterFirstFinishedClient) {
      handle.modules[PARAC_MOD_BROKER]->broker->task_store =
        &task_store_interface;
      handle.modules[PARAC_MOD_BROKER]->broker->compute_node_store =
        &compute_node_store_interface;
    }
    parac_compute_node_store compute_node_store_interface;
    parac_task_store task_store_interface;
    parac::broker::TaskStore taskStore;
    parac::broker::ComputeNodeStore computeNodeStore;
  };

  parac_config_entry* config_entries;
  std::unique_ptr<Internal> internal;

  enum Config {
    AutoShutdownTimeoutMilliseconds,
    AutoShutdownAfterFinishedClient,
    _CONFIG_COUNT
  };
};

static void
init_config(BrokerUserdata* u) {
  parac_config_entry* e = u->config_entries;

  parac_config_entry_set_str(
    &e[BrokerUserdata::AutoShutdownTimeoutMilliseconds],
    "auto-shutdown-timeout",
    "Timeout (in ms) of inactivity in order to automatically exit. 0 "
    "means "
    "off. Only applies to daemon nodes.");
  e[BrokerUserdata::AutoShutdownTimeoutMilliseconds].registrar =
    PARAC_MOD_BROKER;
  e[BrokerUserdata::AutoShutdownTimeoutMilliseconds].type = PARAC_TYPE_UINT32;
  e[BrokerUserdata::AutoShutdownTimeoutMilliseconds].default_value.uint32 = 0;

  parac_config_entry_set_str(
    &e[BrokerUserdata::AutoShutdownAfterFinishedClient],
    "auto-shutdown-after-finished-client",
    "Exit program after the first client that connected sent end signal.");
  e[BrokerUserdata::AutoShutdownAfterFinishedClient].registrar =
    PARAC_MOD_BROKER;
  e[BrokerUserdata::AutoShutdownAfterFinishedClient].type = PARAC_TYPE_SWITCH;
  e[BrokerUserdata::AutoShutdownAfterFinishedClient]
    .default_value.boolean_switch = false;
}

static parac_status
pre_init(parac_module* mod) {
  assert(mod);
  assert(mod->broker);
  assert(mod->handle);
  assert(mod->handle->config);

  BrokerUserdata* userdata = static_cast<BrokerUserdata*>(mod->userdata);

  uint32_t autoShutdownTimeout =
    userdata->config_entries[BrokerUserdata::AutoShutdownTimeoutMilliseconds]
      .value.uint32;

  bool autoShutdownAfterFirstFinishedClient =
    userdata->config_entries[BrokerUserdata::AutoShutdownAfterFinishedClient]
      .value.boolean_switch;

  if(mod->handle->input_file && autoShutdownTimeout > 0) {
    parac_log(PARAC_BROKER,
              PARAC_LOCALWARNING,
              "--auto-shutdown-timeout only applies to daemon nodes and is "
              "deactivated for client nodes!");
    autoShutdownTimeout = 0;
  }
  if(mod->handle->input_file && autoShutdownAfterFirstFinishedClient) {
    parac_log(PARAC_BROKER,
              PARAC_LOCALWARNING,
              "--auto-shutdown-after-finished-client only applies to daemon "
              "nodes and is "
              "deactivated for client nodes!");
    autoShutdownAfterFirstFinishedClient = false;
  }

  userdata->pre_init(
    *mod->handle, autoShutdownTimeout, autoShutdownAfterFirstFinishedClient);

  return PARAC_OK;
}

static parac_status
init(parac_module* mod) {
  assert(mod);
  assert(mod->broker);
  assert(mod->handle);
  assert(mod->handle->config);
  assert(mod->handle->thread_registry);
  assert(mod->userdata);

  BrokerUserdata* userdata = static_cast<BrokerUserdata*>(mod->userdata);

  assert(userdata->internal);

  userdata->internal->computeNodeStore.updateThisNodeDescription();

  return PARAC_OK;
}

static parac_status
mod_request_exit(parac_module* mod) {
  assert(mod);
  assert(mod->broker);
  assert(mod->handle);

  return PARAC_OK;
}

static parac_status
mod_exit(parac_module* mod) {
  assert(mod);
  assert(mod->broker);
  assert(mod->handle);

  if(mod->userdata) {
    BrokerUserdata* userdata = static_cast<BrokerUserdata*>(mod->userdata);
    delete userdata;
  }

  return PARAC_OK;
}

extern "C" PARAC_BROKER_EXPORT parac_status
parac_module_discover_broker(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_BROKER);
  assert(mod);

  mod->type = PARAC_MOD_BROKER;
  mod->name = BROKER_NAME;
  mod->version.major = BROKER_VERSION_MAJOR;
  mod->version.minor = BROKER_VERSION_MINOR;
  mod->version.patch = BROKER_VERSION_PATCH;
  mod->version.tweak = BROKER_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;
  mod->request_exit = mod_request_exit;
  mod->exit = mod_exit;
  mod->handle = handle;

  BrokerUserdata* userdata = new BrokerUserdata();
  mod->userdata = userdata;

  userdata->config_entries = parac_config_reserve(
    handle->config, static_cast<size_t>(BrokerUserdata::_CONFIG_COUNT));
  assert(userdata->config_entries);

  init_config(userdata);

  return PARAC_OK;
}
