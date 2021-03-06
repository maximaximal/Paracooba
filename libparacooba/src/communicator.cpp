#include "../include/paracooba/communicator.hpp"
#include "../include/paracooba/client.hpp"
#include "../include/paracooba/cnf.hpp"
#include "../include/paracooba/config.hpp"
#include "../include/paracooba/daemon.hpp"
#include "../include/paracooba/networked_node.hpp"
#include "../include/paracooba/runner.hpp"

#include "../include/paracooba/messages/message.hpp"
#include "../include/paracooba/messages/node.hpp"
#include "../include/paracooba/net/connection.hpp"
#include "../include/paracooba/net/control.hpp"
#include "../include/paracooba/net/tcp_acceptor.hpp"
#include "../include/paracooba/net/udp_server.hpp"
#include "paracooba/messages/announcement_request.hpp"
#include "paracooba/messages/cnftree_node_status_reply.hpp"
#include "paracooba/messages/cnftree_node_status_request.hpp"
#include "paracooba/messages/jobdescription.hpp"
#include "paracooba/messages/offline_announcement.hpp"
#include "paracooba/messages/online_announcement.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/exception/all.hpp>
#include <boost/system/error_code.hpp>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <regex>
#include <sstream>

#ifdef ENABLE_INTERNAL_WEBSERVER
#include "../include/paracooba/webserver/api.hpp"
#endif

using boost::asio::ip::udp;

namespace paracooba {

static boost::asio::ip::address
ParseBroadcastAddress(Logger& logger, const std::string& addressStr)
{
  boost::system::error_code err;
  auto address = boost::asio::ip::address::from_string(addressStr, err);
  if(err) {
    PARACOOBA_LOG(logger, LocalError)
      << "Could not parse given IP Broadcast Address \"" << addressStr
      << "\". Error: " << err;
    address = boost::asio::ip::address_v4::broadcast();
  }
  return address;
}

static boost::asio::ip::address
ParseIPAddress(Logger& logger, const std::string& addressStr)
{
  boost::system::error_code err;
  auto address = boost::asio::ip::address::from_string(addressStr, err);
  if(err) {
    PARACOOBA_LOG(logger, LocalError) << "Could not parse given IP Address \""
                                      << addressStr << "\". Error: " << err;
    address = boost::asio::ip::address_v4::any();
  }
  return address;
}

Communicator::Communicator(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_ioServiceWork(m_ioService)
  , m_logger(log->createLogger("Communicator"))
  , m_signalSet(std::make_unique<boost::asio::signal_set>(m_ioService, SIGINT))
  , m_clusterStatistics(std::make_shared<ClusterStatistics>(config, log))
  , m_control(std::make_unique<net::Control>(m_ioService,
                                             m_config,
                                             m_log,
                                             *m_clusterStatistics))
  , m_tickTimer(
      (m_ioService),
      std::chrono::milliseconds(m_config->getUint64(Config::TickMilliseconds)))
{
  m_config->m_communicator = this;
  m_signalSet->async_wait(std::bind(&Communicator::signalHandler,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));

  if(config->isInternalWebserverEnabled()) {
    m_webserverInitiator =
      std::make_unique<webserver::Initiator>(config, log, m_ioService);
  }
}

Communicator::~Communicator()
{
  m_config->m_communicator = nullptr;
  exit();
  PARACOOBA_LOG(m_logger, Trace) << "Destruct Communicator.";
}

void
Communicator::run()
{
  using namespace boost::asio;

  // Handle known remotes after startup.
  Config::StringVector knownRemotes =
    m_config->getStringVector(Config::KnownRemotes);
  readRemotesFromEnvironment(knownRemotes);

  if(knownRemotes.size()) {
    m_config->setEnableAutoDiscovery(false);
  }

  m_control->setJobDescriptionReceiverProvider(
    getJobDescriptionReceiverProvider());

  if(!listenForIncomingUDP(m_config->getUint16(Config::UDPListenPort)))
    return;

  m_clusterStatistics->setStatelessMessageTransmitter(*m_udpServer);
  m_clusterStatistics->initLocalNode();

  if(!listenForIncomingTCP(m_config->getUint16(Config::TCPListenPort)))
    return;

  if(!m_runner) {
    m_runner = std::make_shared<Runner>(this, m_config, m_log);
  }
  if(!m_runner->isRunning()) {
    m_runner->start();
  }

  if(m_config->autoDiscoveryEnabled()) {
    m_udpServer->startAccepting(*m_clusterStatistics,
                                m_clusterStatistics->getThisNode());
  }
  m_tcpAcceptor->startAccepting();
  if(m_config->autoDiscoveryEnabled()) {
    messages::Message announcementRequestMsg =
      m_udpServer->buildMessage(*m_config);
    messages::AnnouncementRequest announcementRequest(m_config->buildNode());
    announcementRequestMsg.insert(announcementRequest);
    m_udpServer->broadcastMessage(announcementRequestMsg);
  } else {
    PARACOOBA_LOG(m_logger, Debug)
      << "Auto Discovery disabled! Only known remotes are used. UDP packets "
         "are ignored.";
  }

  if(m_webserverInitiator) {
    m_ioService.post(
      std::bind(&webserver::Initiator::run, m_webserverInitiator.get()));
  }

  if(knownRemotes.size() > 0) {
    m_ioService.post([this, &knownRemotes]() {
      PARACOOBA_LOG(m_logger, Debug) << "Connecting to known remote hosts, "
                                        "because known remotes were specified.";
      for(const std::string& remote : knownRemotes) {
        net::Connection connection(m_ioService,
                                   m_log,
                                   m_config,
                                   *m_clusterStatistics,
                                   *m_control,
                                   getJobDescriptionReceiverProvider());
        connection.connect(remote);
      }
    });
  }

  // The timer can only be enabled at this stage, after all other required data
  // structures have been initialised. Also, use a warmup time of 2 seconds -
  // the first 5 seconds do not need this, because other work has to be done.
  m_tickTimer.expires_from_now(std::chrono::seconds(2));
  m_tickTimer.async_wait(
    std::bind(&Communicator::tick, this, std::placeholders::_1));

  PARACOOBA_LOG(m_logger, Trace) << "Communicator io_service started.";
  bool ioServiceRunningWithoutException = true;
  while(ioServiceRunningWithoutException) {
    try {
      m_ioService.run();
      ioServiceRunningWithoutException = false;
    } catch(const boost::exception& e) {
      PARACOOBA_LOG(m_logger, LocalError)
        << "Boost exception encountered from ioService! Diagnostics info: "
        << boost::diagnostic_information(e);
    } catch(const std::exception& e) {
      PARACOOBA_LOG(m_logger, LocalError)
        << "Exception encountered from ioService! Message: " << e.what();
    }
  }
  // Run last milliseconds to try to send offline announcements.
  PARACOOBA_LOG(m_logger, Trace) << "Communicator io_service ended.";
  m_runner->stop();
}

void
Communicator::exit()
{
  if(m_runner->m_running) {
    m_config->setStopping(true);

    auto [nodeMap, lock] = m_clusterStatistics->getNodeMap();
    for(auto& it : nodeMap) {
      auto& node = it.second;
      if(node.getId() == m_config->getId())
        continue;

      NetworkedNode* nn = node.getNetworkedNode();
      assert(nn);
      if(nn->getConnectionReadyWaiter().isReady()) {
        nn->getConnection()->sendEndToken();
      }
      nn->offlineAnnouncement(*m_config, *nn, "Shutdown");
    }
  }

  // Early stop m_runner, so that threads must not be notified more often if
  // this is called from a runner thread.
  m_runner->m_running = false;

  PARACOOBA_LOG(m_logger, Trace)
    << "Shutting down after waiting for last messages to be sent (100ms).";
  m_tickTimer.cancel();
  m_tickTimer.expires_from_now(std::chrono::milliseconds(100));
  m_tickTimer.async_wait([this](const boost::system::error_code& ec) {
    if(m_webserverInitiator) {
      m_webserverInitiator->stop();
      m_webserverInitiator.reset();
    }
    m_runner->stop();

    // Destruct all servers before io Service is stopped.
    m_udpServer.reset();
    m_tcpAcceptor.reset();

    m_ioService.stop();
  });
}

void
Communicator::startRunner()
{
  if(!m_runner) {
    m_runner = std::make_shared<Runner>(this, m_config, m_log);
  }
  m_runner->start();
}

void
Communicator::signalHandler(const boost::system::error_code& error,
                            int signalNumber)
{
  if(signalNumber == SIGINT) {
    PARACOOBA_LOG(m_logger, Trace) << "SIGINT detected.";
    exit();
  }
}

void
Communicator::checkAndTransmitClusterStatisticsChanges(bool force)
{
  if(m_clusterStatistics->clearChanged() || force) {
#ifdef ENABLE_INTERNAL_WEBSERVER
    // Transmit changes to API, so all web-clients can see the new cluster
    // statistics.
    if(m_webserverInitiator) {
      auto api = m_webserverInitiator->getAPI();
      if(api) {
        api->injectClusterStatisticsUpdate(*m_clusterStatistics);
      }
    }
#endif
  }
}

messages::JobDescriptionReceiverProvider&
Communicator::getJobDescriptionReceiverProvider()
{
  if(m_config->isDaemonMode()) {
    return *m_config->getDaemon();
  } else {
    return *m_config->getClient();
  }
}

bool
Communicator::listenForIncomingUDP(uint16_t port)
{
  using namespace boost::asio;
  try {
    auto udpEndpoint = boost::asio::ip::udp::endpoint(
      ParseIPAddress(m_logger,
                     std::string(m_config->getString(Config::IPAddress))),
      m_config->getUint16(Config::UDPListenPort));
    auto udpBroadcastEndpoint = boost::asio::ip::udp::endpoint(
      ParseIPAddress(
        m_logger, std::string(m_config->getString(Config::IPBroadcastAddress))),
      m_config->getUint16(Config::UDPTargetPort));

    m_udpServer = std::make_unique<net::UDPServer>(m_ioService,
                                                   udpEndpoint,
                                                   udpBroadcastEndpoint,
                                                   m_config,
                                                   m_log,
                                                   *m_control);
    return true;
  } catch(std::exception& e) {
    PARACOOBA_LOG(m_logger, LocalError)
      << "Could not initialize server for incoming UDP connections on port "
      << port << "! Error: " << e.what();
    return false;
  }
}

bool
Communicator::listenForIncomingTCP(uint16_t port)
{
  using namespace boost::asio;
  try {
    auto tcpEndpoint = boost::asio::ip::tcp::endpoint(
      ParseIPAddress(m_logger,
                     std::string(m_config->getString(Config::IPAddress))),
      m_config->getUint16(Config::TCPListenPort));

    m_tcpAcceptor =
      std::make_unique<net::TCPAcceptor>(m_ioService,
                                         tcpEndpoint,
                                         m_log,
                                         m_config,
                                         *m_clusterStatistics,
                                         *m_control,
                                         getJobDescriptionReceiverProvider());
    return true;
  } catch(std::exception& e) {
    PARACOOBA_LOG(m_logger, LocalError)
      << "Could not initialize server for incoming TCP connections on port "
      << port << "! Error: " << e.what();
    return false;
  }
}

void
Communicator::injectCNFTreeNodeInfo(int64_t cnfId,
                                    int64_t handle,
                                    Path p,
                                    CNFTree::State state,
                                    int64_t remote)
{
#ifdef ENABLE_INTERNAL_WEBSERVER
  webserver::API* api = m_webserverInitiator->getAPI();
  if(!api) {
    PARACOOBA_LOG(m_logger, LocalWarning)
      << "Cannot inject CNFTreeNodeInfo into uninitialised webserver::API!";
    return;
  }
  api->injectCNFTreeNode(handle, p, state, remote);
#endif
}

void
Communicator::requestCNFTreePathInfo(
  const messages::CNFTreeNodeStatusRequest& request)
{
  std::shared_ptr<CNF> cnf = GetRootCNF(m_config.get(), request.getCnfId());
  if(!cnf)
    return;
  CNFTree& cnfTree = cnf->getCNFTree();

  Path p = request.getPath();

  std::shared_ptr<NetworkedNode> nn = cnfTree.getOffloadTargetNetworkedNode(p);

  if(!nn) {
    // Handled locally, can directly insert local information, if this should be
    // sent to a remote or inserted into local info if requested locally.

    if(request.getHandleStack().size() == 1) {
      // Handle this request locally.
      injectCNFTreeNodeInfo(request.getCnfId(),
                            request.getHandle(),
                            p,
                            cnfTree.getState(p),
                            m_config->getInt64(Config::Id));
    } else {
      // Build answer message.
      messages::Message replyMsg;
      messages::CNFTreeNodeStatusReply reply(m_config->getInt64(Config::Id),
                                             request);
      reply.addNode(request.getPath(), cnfTree.getState(request.getPath()));
      replyMsg.insertCNFTreeNodeStatusReply(std::move(reply));
      // TODO
      // m_udpServer->sendMessage(request.getHandle(), replyMsg, false);
    }
  } else {
    messages::Message requestMsg;
    messages::CNFTreeNodeStatusRequest request(m_config->getInt64(Config::Id),
                                               request);
    requestMsg.insert(std::move(request));
    // TODO
    // m_udpServer->sendMessage(targetNode, requestMsg, false);
  }
}

void
Communicator::tick(const boost::system::error_code& ec)
{
  if(ec == boost::asio::error::operation_aborted)
    return;

  assert(m_runner);
  assert(m_udpServer);
  assert(m_clusterStatistics);

  m_runner->checkTaskFactories();

  assert(m_clusterStatistics);
  m_clusterStatistics->tick();

  // Update workQueueSize
  auto& thisNode = m_clusterStatistics->getThisNode();
  thisNode.setWorkQueueSize(m_runner->getWorkQueueSize());
  {
    auto [v, lock] = m_runner->getTaskFactories();
    thisNode.applyTaskFactoryVector(v);
  }
  checkAndTransmitClusterStatisticsChanges();

  sendStatusToAllPeers();

  m_clusterStatistics->rebalance();

  m_tickTimer.expires_from_now(
    std::chrono::milliseconds(m_config->getUint64(Config::TickMilliseconds)));
  m_tickTimer.async_wait(
    std::bind(&Communicator::tick, this, std::placeholders::_1));
}

void
Communicator::sendStatusToAllPeers()
{
  messages::NodeStatus::OptionalDaemon optionalDaemon = std::nullopt;

  if(m_config->isDaemonMode()) {
    auto daemon = m_config->getDaemon();
    assert(daemon);

    messages::Daemon daemonMsg;

    auto [contextMap, lock] = daemon->getContextMap();

    for(auto& it : contextMap) {
      auto& context = *it.second;
      messages::DaemonContext ctx(context.getOriginatorId(),
                                  static_cast<uint8_t>(context.getState()),
                                  context.getFactoryQueueSize());
      daemonMsg.addContext(std::move(ctx));
    }

    optionalDaemon = daemonMsg;
  } else {
    auto client = m_config->getClient();
    assert(client);
  }

  messages::NodeStatus nodeStatus(m_runner->getWorkQueueSize(), optionalDaemon);
  messages::Message msg = m_udpServer->buildMessage(*m_config);
  msg.insert(std::move(nodeStatus));

  // Send built message to all other known nodes.
  sendToSelectedPeers(msg, [](const ClusterNode&) { return true; });
}

void
conditionalReadIntoStrVectFromEnv(Logger& l,
                                  std::vector<std::string>& vect,
                                  const char* env)
{
  const char* var = ::std::getenv(env);
  if(var != nullptr) {
    PARACOOBA_LOG(l, Info) << "Adding known remote node \"" << var
                           << "\" from environment variable \"" << env << "\".";
    vect.push_back(var);
  }
}

void
Communicator::readRemotesFromEnvironment(std::vector<std::string>& vect)
{
  // https://github.com/aws-samples/aws-batch-comp-infrastructure-sample#run-solversh
  conditionalReadIntoStrVectFromEnv(
    m_logger, vect, "AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS");

  // General master host envvar.
  conditionalReadIntoStrVectFromEnv(m_logger, vect, "PARACOOBA_MASTER_HOST");
}

void
Communicator::sendToSelectedPeers(const messages::Message& msg,
                                  const ClusterNodePredicate& predicate)
{
  auto [nodeMap, lock] = m_clusterStatistics->getNodeMap();
  for(auto& it : nodeMap) {
    auto& node = it.second;
    if(node.getId() != m_config->getInt64(Config::Id)) {
      if(predicate(node)) {
        NetworkedNode* nn = node.getNetworkedNode();
        nn->transmitMessage(msg, *nn);
      }
    }
  }
}
}
