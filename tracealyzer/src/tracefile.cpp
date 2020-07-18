#include "tracefile.hpp"
#include "tracefileview.hpp"
#include <algorithm>
#include <list>
#include <set>

using std::cerr;
using std::clog;
using std::cout;
using std::endl;

namespace paracooba::tracealyzer {
TraceFile::TraceFile(const std::string& path)
{
  sink.open(path);
  if(!sink.is_open()) {
    std::cerr << "!! Could not open file as memory mapped file!" << std::endl;
    exit(EXIT_FAILURE);
  }

  byteSize = sink.size();
  entries = byteSize / sizeof(TraceEntry);

  std::clog << "-> Opened trace of size " << BytePrettyPrint(byteSize)
            << ", containing " << entries << " trace entries." << std::endl;

  if(size() > 0) {
    TraceEntry& first = (*this)[0];
    if(first.kind != traceentry::Kind::ClientBegin ||
       !first.body.clientBegin.sorted) {
      sort();
      if(first.kind != traceentry::Kind::ClientBegin) {
        std::cerr << "!! First entry not of kind ClientBegin! Is the data "
                     "correct? Entry: "
                  << first << std::endl;
      } else {
        first.body.clientBegin.sorted = true;
      }
    }
  }
}
TraceFile::~TraceFile() {}

void
TraceFile::sort()
{
  std::clog << "  => Begin sorting... ";
  std::sort(begin(), end());
  std::clog << " sorting finished! " << std::endl;
  std::clog << "  => Begin causal sorting... " << endl;
  causalSort();
  std::clog << "  => causal sorting finished! " << std::endl;
}

traceentry::Kind
inverseKind(traceentry::Kind kind)
{
  switch(kind) {
    case traceentry::Kind::RecvMsg:
      return traceentry::Kind::SendMsg;
    case traceentry::Kind::SendMsg:
      return traceentry::Kind::RecvMsg;
    case traceentry::Kind::SendResult:
      return traceentry::Kind::ReceiveResult;
    case traceentry::Kind::ReceiveResult:
      return traceentry::Kind::SendResult;
    case traceentry::Kind::OffloadTask:
      return traceentry::Kind::ReceiveTask;
    case traceentry::Kind::ReceiveTask:
      return traceentry::Kind::OffloadTask;
    default:
      assert(false);
      return traceentry::Kind::ComputeNodeDescription;
  }
}

ID
getRegardingID(const TraceEntry& e)
{
  switch(e.kind) {
    case traceentry::Kind::RecvMsg:
      return e.body.recvMsg.sender;
    case traceentry::Kind::SendMsg:
      return e.body.sendMsg.target;
    case traceentry::Kind::SendResult:
      return e.body.sendResult.target;
    case traceentry::Kind::ReceiveResult:
      return e.body.receiveResult.source;
    case traceentry::Kind::OffloadTask:
      return e.body.offloadTask.target;
    case traceentry::Kind::ReceiveTask:
      return e.body.receiveTask.source;
    default:
      return 0;
  }
}

void
TraceFile::causalSort()
{
  std::list<traceentry::Kind> openedKinds;
  std::set<ID> knownHosts;

  for(size_t i = 0; i < entries; ++i) {
    TraceEntry& e = (*this)[i];

    switch(e.kind) {
      case traceentry::Kind::ComputeNodeDescription:
        knownHosts.insert(e.thisId);
        break;

      case traceentry::Kind::SendMsg:
      case traceentry::Kind::SendResult:
      case traceentry::Kind::OffloadTask:
        if(!knownHosts.count(getRegardingID(e))) {
          continue;
        }
        openedKinds.push_front(e.kind);
        break;
      case traceentry::Kind::RecvMsg:
      case traceentry::Kind::ReceiveTask:
      case traceentry::Kind::ReceiveResult:
        // If the remote ID is not yet known, the entry is not causally sorted,
        // as there will be no swap target.
        if(!knownHosts.count(getRegardingID(e))) {
          continue;
        }

        // No specific check is carried out if a fitting entry was first opened,
        // as this should be the normal case. The basic ping mechanism already
        // brings the time close together.
        {
          auto it = std::find(
            openedKinds.begin(), openedKinds.end(), inverseKind(e.kind));
          if(it != openedKinds.end()) {
            openedKinds.erase(it);
          } else {
            clog << "   -> Causal fixup required for early " << e << "..."
                 << endl;
            if(causalFixup(i)) {
              // As the two are now swapped, e.kind is now the correct one and
              // that kind can be used to open a new scope.
              openedKinds.push_front(e.kind);
            } else {
              cerr
                << "!! Could not find matching entry to causally fixup early "
                << e << "!" << endl;
            }
          }
        }
        break;
      default:
        // No other important causal relationship is checked.
        break;
    }
  }
}

bool
TraceFile::causalFixup(size_t i)
{
  TraceEntry& e = (*this)[i];

  size_t searchPos = i + 1;
  bool matchFound = false;
  while(!matchFound && searchPos != 0) {
    searchPos = forwardSearchForKind(searchPos, inverseKind(e.kind));

    if(searchPos != 0) {
      TraceEntry& possibleMatch = (*this)[searchPos];

      switch(e.kind) {
        case traceentry::Kind::RecvMsg:
          assert(possibleMatch.kind == traceentry::Kind::SendMsg);
          matchFound = possibleMatch.body.sendMsg.kind == e.body.recvMsg.kind &&
                       possibleMatch.body.sendMsg.udp == e.body.recvMsg.udp &&
                       possibleMatch.thisId == e.body.recvMsg.sender;
          break;
        case traceentry::Kind::ReceiveTask:
          assert(possibleMatch.kind == traceentry::Kind::OffloadTask);
          matchFound =
            possibleMatch.body.offloadTask.path == e.body.receiveTask.path &&
            possibleMatch.body.offloadTask.target == e.thisId;
          break;
        case traceentry::Kind::ReceiveResult:
          assert(possibleMatch.kind == traceentry::Kind::SendResult);
          matchFound =
            possibleMatch.body.sendResult.path == e.body.receiveResult.path &&
            possibleMatch.body.sendResult.state == e.body.receiveResult.state &&
            possibleMatch.body.sendResult.target == e.thisId;
          break;
        default:
          break;
      }

      ++searchPos;
    }
  }

  if(matchFound) {
    swap(i, searchPos);
  }

  return matchFound;
}

TraceFileView
TraceFile::getOnlyTraceKind(traceentry::Kind kind)
{
  size_t i = 0;
  for(i = 0; (*this)[i].kind != kind && i < entries; ++i) {
  }

  TraceFileView view(
    *this,
    i,
    [](TraceEntry& e, void* _, uint64_t userdata2) {
      traceentry::Kind kind = static_cast<traceentry::Kind>(userdata2);
      return e.kind == kind;
    },
    nullptr,
    static_cast<uint64_t>(kind));
  return view;
}

void
TraceFile::printUtilizationLog()
{
  for(auto& e : (*this)) {
    switch(e.kind) {
      case traceentry::Kind::ComputeNodeDescription:
        clog << e << endl;
        break;
      case traceentry::Kind::WorkerIdle:
        clog << e << endl;
        break;
      case traceentry::Kind::WorkerWorking:
        clog << e << endl;
        break;
      default:
        continue;
    }
  }
}

void
TraceFile::printNetworkLog()
{
  for(auto& e : (*this)) {
    switch(e.kind) {
      case traceentry::Kind::ComputeNodeDescription:
        clog << e << endl;
        break;
      case traceentry::Kind::ClientBegin:
        clog << e << endl;
        break;
      case traceentry::Kind::SendMsg:
        clog << e << endl;
        break;
      case traceentry::Kind::RecvMsg:
        clog << e << endl;
        break;
      case traceentry::Kind::ReceiveTask:
        clog << e << endl;
        break;
      case traceentry::Kind::OffloadTask:
        clog << e << endl;
        break;
      case traceentry::Kind::SendResult:
        clog << e << endl;
        break;
      case traceentry::Kind::ReceiveResult:
        clog << e << endl;
        break;
      default:
        continue;
    }
  }
}

void
TraceFile::swap(size_t i1, size_t i2)
{
  assert(i1 < entries);
  assert(i2 < entries);
  TraceEntry* e1 = &(*this)[i1];
  TraceEntry* e2 = &(*this)[i2];

  std::swap(*e1, *e2);
  std::swap(e1->nsSinceStart, e2->nsSinceStart);
}

size_t
TraceFile::forwardSearchForKind(size_t start, traceentry::Kind kind)
{
  if(start >= entries)
    return 0;

  assert(start < entries);

  for(size_t i = start; i < entries; ++i) {
    TraceEntry& e = (*this)[i];
    if(e.kind == kind) {
      return i;
    }
  }
  return 0;
}
}
