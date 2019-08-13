@0xead81247730f0294;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("paracuber::message");

struct Node
{
  name @0 : Text; # Name of the node
  id @1 : Int64; # Id of the node
  availableWorkers @2 : UInt16; # number of available worker threads on the node
  workQueueCapacity @3 : UInt64; # number of tasks the work queue can hold
  workQueueSize @4 : UInt64; # number of tasks currently in the work queue, like in the status message
  maximumCPUFrequency @5 : UInt16; # maximum frequency of the cpu on this node
  uptime @6 : UInt32; # uptime (in seconds) of the node
  udpListenPort @7 : UInt16; # listen port for udp control messages
}

struct AnnouncementRequest
{
  requester @0 : Node;

  nameMatch : union {
    noRestriction @1 : Void;
    regex @2 : Text;
    id @3: Int64;
  }
}
# Request announcements of online nodes with given
# restrictions. Announcement should be sent per unicast.

struct OnlineAnnouncement
{
  node @0 : Node;
  knownNodes @1 : List(Node);
}
# Announce a node is online. May be sent regularily or
# after issuing an AnnouncementRequest.

struct OfflineAnnouncement
{
  reason @0 : Text;
}
# Announce a node is coming down and going offline.

struct NodeStatus
{
  workQueueSize @0 : UInt64; # number of tasks in worker queue
}
# An update about node statistics. Sent to all other
# known online nodes approximately every second.


struct Message
{
  id @0 : Int16;
  # Message ID, starting at - 2^15 and increasing up to 2 ^ 15 - 1, then wrapping around
  origin @1 : Int64;
  # Id of the sender. Can be generated by using PID + MAC Address or PID (16 Bit) + 48 Bit Unique Number.

  union
  {
    announcementRequest @2 : AnnouncementRequest;
    onlineAnnouncement @3 : OnlineAnnouncement;
    offlineAnnouncement @4 : OfflineAnnouncement;
    nodeStatus @5 : NodeStatus;
  }
  # acual message body
}