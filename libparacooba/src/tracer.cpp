#ifdef PARACOOBA_ENABLE_TRACING_SUPPORT

#include "../include/paracooba/tracer.hpp"
#include <boost/filesystem.hpp>
#include <cassert>
#include <string>
#include <thread>

namespace paracooba {
thread_local Tracer::OutHandle Tracer::m_outHandle;

Tracer::Tracer() {}
Tracer::~Tracer() {}

void
Tracer::setOutputPath(const std::string_view& path)
{
  m_outputPath = path;
  assert(m_thisId != 0);
  m_active = path != "";

  if(m_active) {
    assert(boost::filesystem::exists(m_outputPath));
    m_outputPath += "/paracooba_trace_" + std::to_string(m_thisId) + "/";
    assert(!boost::filesystem::exists(m_outputPath));
    boost::filesystem::create_directory(m_outputPath);
  }
}
void
Tracer::setThisId(ID thisId)
{
  assert(m_thisId == 0);
  m_thisId = thisId;
}

void
Tracer::logEntry(const TraceEntry& e)
{
  if(!m_outHandle.outStream.is_open()) {
    m_outHandle.path =
      m_outputPath + "paracooba_thread_trace_" +
      std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) +
      ".bin";
    m_outHandle.outStream.open(m_outHandle.path,
                               std::ofstream::out | std::ofstream::app);
  }

  // Just directly write the binary structure to the file for the current
  // thread.
  m_outHandle.outStream.write(reinterpret_cast<const char*>(&e), sizeof(e));
}

void
Tracer::resetStart(uint64_t offset)
{
  // Reset the start time to be now with the specified offset. This is called on
  // first contact with a client, to synchronize the daemon clock with the
  // client.
  get().m_startTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count() -
                      offset;

  get().m_active = get().m_outputPath != "";
}
}

#endif
