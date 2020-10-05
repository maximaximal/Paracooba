#include "service.hpp"

#include <boost/asio/io_context.hpp>

#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>
#include <paracooba/module.h>

using boost::asio::io_context;

namespace parac::communicator {
struct Service::Internal {
  io_context context;
  parac_thread_registry_handle threadHandle;
};

Service::Service(struct parac_handle& handle)
  : m_internal(std::make_unique<Internal>())
  , m_handle(handle) {
  m_internal->threadHandle.userdata = this;
}

Service::~Service() {}

parac_status
Service::start() {
  return parac_thread_registry_create(
    m_handle.thread_registry,
    m_handle.modules[PARAC_MOD_COMMUNICATOR],
    [](parac_thread_registry_handle* handle) -> int {
      Service* service = static_cast<Service*>(handle->userdata);
      return service->run();
    },
    &m_internal->threadHandle);
}

parac_status
Service::run() {
  parac_log(
    PARAC_COMMUNICATOR, PARAC_DEBUG, "Starting communicator io_context.");
  m_internal->context.run();
  return PARAC_OK;
}

io_context&
Service::ioContext() {
  return m_internal->context;
}
}
