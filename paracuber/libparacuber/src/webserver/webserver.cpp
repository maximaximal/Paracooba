#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/config.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "../../include/paracuber/config.hpp"
#include "../../include/paracuber/webserver/api.hpp"
#include "../../include/paracuber/webserver/webserver.hpp"

#ifndef ENABLE_INTERNAL_WEBSERVER
#error "Internal Webserver must be enabled if this source is compiled!"
#endif

using namespace std::placeholders;
using tcp = boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = boost::beast::http;
namespace asio = boost::asio;

// The server in this file is inspired from the async HTTP server example over
// here:
// https://www.boost.org/doc/libs/1_66_0/libs/beast/example/http/server/async/http_server_async.cpp

namespace paracuber {
namespace webserver {

inline std::string
boostStringViewToStdString(boost::string_view view)
{
  return std::move(std::string(view.data(), view.size()));
}

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view
mime_type(boost::beast::string_view path)
{
  using boost::beast::iequals;
  auto const ext = [&path] {
    auto const pos = path.rfind(".");
    if(pos == boost::beast::string_view::npos)
      return boost::beast::string_view{};
    return path.substr(pos);
  }();
  if(iequals(ext, ".htm"))
    return "text/html";
  if(iequals(ext, ".html"))
    return "text/html";
  if(iequals(ext, ".php"))
    return "text/html";
  if(iequals(ext, ".css"))
    return "text/css";
  if(iequals(ext, ".txt"))
    return "text/plain";
  if(iequals(ext, ".js"))
    return "application/javascript";
  if(iequals(ext, ".json"))
    return "application/json";
  if(iequals(ext, ".xml"))
    return "application/xml";
  if(iequals(ext, ".swf"))
    return "application/x-shockwave-flash";
  if(iequals(ext, ".flv"))
    return "video/x-flv";
  if(iequals(ext, ".png"))
    return "image/png";
  if(iequals(ext, ".jpe"))
    return "image/jpeg";
  if(iequals(ext, ".jpeg"))
    return "image/jpeg";
  if(iequals(ext, ".jpg"))
    return "image/jpeg";
  if(iequals(ext, ".gif"))
    return "image/gif";
  if(iequals(ext, ".bmp"))
    return "image/bmp";
  if(iequals(ext, ".ico"))
    return "image/vnd.microsoft.icon";
  if(iequals(ext, ".tiff"))
    return "image/tiff";
  if(iequals(ext, ".tif"))
    return "image/tiff";
  if(iequals(ext, ".svg"))
    return "image/svg+xml";
  if(iequals(ext, ".svgz"))
    return "image/svg+xml";
  return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
pathCat(boost::beast::string_view base, boost::beast::string_view path)
{
  if(base.empty())
    return path.to_string();
  std::string result = base.to_string();
#if BOOST_MSVC
  char constexpr path_separator = '\\';
  if(result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for(auto& c : result)
    if(c == '/')
      c = path_separator;
#else
  char constexpr path_separator = '/';
  if(result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
#endif
  return result;
}

using Websocket =
  boost::beast::websocket::stream<boost::asio::ip::tcp::socket&>;

template<class Body, class Allocator, class Send>
void
handleRequest(Webserver* webserver,
              boost::beast::string_view doc_root,
              http::request<Body, http::basic_fields<Allocator>>&& req,
              Send&& send)
{
  // Returns a bad request response
  auto const bad_request = [&req](boost::beast::string_view why) {
    http::response<http::string_body> res{ http::status::bad_request,
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = why.to_string();
    res.prepare_payload();
    return res;
  };

  // Returns a not found response
  auto const not_found = [&req](boost::beast::string_view target) {
    http::response<http::string_body> res{ http::status::not_found,
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + target.to_string() + "' was not found.";
    res.prepare_payload();
    return res;
  };

  // Returns a server error response
  auto const server_error = [&req](boost::beast::string_view what) {
    http::response<http::string_body> res{ http::status::internal_server_error,
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + what.to_string() + "'";
    res.prepare_payload();
    return res;
  };

  // Returns a server error response
  auto const api_response = [&req, webserver](API::Request request) {
    assert(webserver);
    http::response<http::string_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    res.body() = webserver->getAPI()->generateResponse(request);
    res.prepare_payload();
    return res;
  };

  // Make sure we can handle the method
  if(req.method() != http::verb::get && req.method() != http::verb::head)
    return send(bad_request("Unknown HTTP-method"));

  // Request path must be absolute and not contain "..".
  if(req.target().empty() || req.target()[0] != '/' ||
     req.target().find("..") != boost::beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  // Build the path to the requested file
  std::string path = pathCat(doc_root, req.target());
  if(req.target().back() == '/')
    path.append("index.html");

  auto targetPath = boostStringViewToStdString(req.target());
  if(API::isAPIRequest(targetPath)) {
    return send(api_response(API::matchTargetToRequest(targetPath)));
  }

  // Attempt to open the file
  boost::beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), boost::beast::file_mode::scan, ec);

  // Handle the case where the file doesn't exist
  if(ec == boost::system::errc::no_such_file_or_directory)
    return send(not_found(req.target()));

  // Handle an unknown error
  if(ec)
    return send(server_error(ec.message()));

  // Respond to HEAD request
  if(req.method() == http::verb::head) {
    http::response<http::empty_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(body.size());
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
  }

  // Respond to GET request
  http::response<http::file_body> res{ std::piecewise_construct,
                                       std::make_tuple(std::move(body)),
                                       std::make_tuple(http::status::ok,
                                                       req.version()) };
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, mime_type(path));
  res.content_length(body.size());
  res.keep_alive(req.keep_alive());
  return send(std::move(res));
}

class Webserver::HTTPSession
  : public std::enable_shared_from_this<Webserver::HTTPSession>
{
  public:
  HTTPSession(Webserver* webserver,
              tcp::socket socket,
              std::string const& docRoot)
    : m_webserver(webserver)
    , m_logger(m_webserver->m_log->createLogger())
    , m_socket(std::move(socket))
    , m_strand(m_socket.get_executor())
    , m_docRoot(docRoot)
    , m_lambda(*this)
  {
    assert(m_webserver);
  }
  ~HTTPSession()
  {
    PARACUBER_LOG(m_logger, Trace) << "HTTP TCP Session ended.";
    if(m_webserver) {
      m_webserver->httpSessionClosed(this);
      if(m_webserver->getAPI()) {
        m_webserver->getAPI()->handleWebSocketRequest(
          &m_socket,
          std::bind(
            &HTTPSession::wsSendPropertyTree, this, std::placeholders::_1),
          nullptr);
      }
    }
  }

  void run()
  {
    PARACUBER_LOG(m_logger, Trace) << "New HTTP TCP Session established from "
                                   << m_socket.remote_endpoint() << ".";
    doRead();
  }

  boost::property_tree::ptree makeErrorPTree(const std::string& message)
  {
    boost::property_tree::ptree ptree;
    ptree.put("type", "error");
    ptree.put("message", message);
    return std::move(ptree);
  }

  void wsRun()
  {
    m_websocket->async_accept(
      m_req,
      boost::asio::bind_executor(m_strand,
                                 std::bind(&HTTPSession::wsOnAccept,
                                           shared_from_this(),
                                           std::placeholders::_1)));
  }
  void wsOnAccept(beast::error_code ec)
  {
    if(ec != boost::system::errc::success) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not accept Websocket! Error: " << ec.message();
      return;
    }

    PARACUBER_LOG(m_logger, Trace)
      << "Accept websocket from " << m_socket.remote_endpoint();

    // Read a message
    wsDoRead();
  }
  void wsDoRead()
  {
    m_websocket->async_read(
      m_buffer,
      boost::asio::bind_executor(m_strand,
                                 std::bind(&HTTPSession::wsOnRead,
                                           shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2)));
  }

  void wsOnRead(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if(ec == websocket::error::closed)
      return;

    if(ec != boost::system::errc::success) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not read Websocket! Error: " << ec.message();
      return;
    }

    if(m_websocket->got_text()) {
      assert(m_webserver);
      boost::property_tree::ptree ptree;
      std::string msg = beast::buffers_to_string(m_buffer.data());
      m_buffer.consume(m_buffer.size());
      std::istringstream iss(msg);
      try {
        boost::property_tree::json_parser::read_json(iss, ptree);

        m_webserver->getAPI()->handleWebSocketRequest(
          &m_socket,
          std::bind(
            &HTTPSession::wsSendPropertyTree, this, std::placeholders::_1),
          &ptree);
      } catch(boost::property_tree::ptree_error& e) {
        PARACUBER_LOG(m_logger, LocalWarning)
          << "Could not parse received JSON from WebSocket! Data: \"" << msg
          << "\" Error: " << e.what();

        auto ptree = makeErrorPTree(
          std::string("Could not parse request! Error: ") + e.what());
        wsSendPropertyTree(ptree);
      }
    }

    wsDoRead();
  }

  void wsOnWrite(boost::system::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);
    m_buffer.consume(m_buffer.size());

    // Happens when the timer closes the socket
    if(ec == boost::asio::error::operation_aborted)
      return;

    if(ec != boost::system::errc::success) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not write to Websocket! Error: " << ec.message();
      return;
    }
  }
  void wsSendPropertyTree(boost::property_tree::ptree& tree)
  {
    assert(m_webserver);
    m_webserver->getIOService().post([this, tree]() {
      auto os = boost::beast::ostream(m_buffer);
      boost::property_tree::json_parser::write_json(os, tree);
      m_websocket->text(true);

      m_websocket->async_write(
        m_buffer.data(),
        boost::asio::bind_executor(m_strand,
                                   std::bind(&HTTPSession::wsOnWrite,
                                             shared_from_this(),
                                             std::placeholders::_1,
                                             std::placeholders::_2)));
    });
  }

  void doRead()
  {
    // Read a request
    http::async_read(
      m_socket,
      m_buffer,
      m_req,
      boost::asio::bind_executor(m_strand,
                                 std::bind(&HTTPSession::onRead,
                                           shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2)));
  }

  void onRead(boost::system::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if(ec == http::error::end_of_stream)
      return doClose();

    if(ec != boost::system::errc::success) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not read HTTP! Error: " << ec.message();
    }

    if(boost::beast::websocket::is_upgrade(m_req)) {
      // This is a websocket request and should be treated as such.
      m_websocket = std::make_unique<Websocket>(m_socket);
      wsRun();
      return;
    }

    handleRequest(m_webserver, m_docRoot, std::move(m_req), m_lambda);

    doRead();
  }

  boost::asio::ip::tcp::socket& getTCPSocket() { return m_socket; }

  void onWrite(boost::system::error_code ec,
               std::size_t bytes_transferred,
               bool close)
  {
    boost::ignore_unused(bytes_transferred);

    if(ec != boost::system::errc::success) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not write HTTP! Error: " << ec.message();
    }

    if(close) {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return doClose();
    }

    // We're done with the response so delete it
    m_res = nullptr;

    // Read another request
    doRead();
  }

  void doClose()
  {
    if(m_socket.is_open())
      return;

    // Send a TCP shutdown
    boost::system::error_code ec;
    m_socket.shutdown(tcp::socket::shutdown_send, ec);

    if(ec != boost::system::errc::success) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not close HTTP TCP socket! Error: " << ec.message();
    }

    // At this point the connection is closed gracefully
    m_socket.close();
  }

  void setWebserver(Webserver* server) { m_webserver = server; }

  private:
  struct send_lambda
  {
    HTTPSession& m_self;

    explicit send_lambda(HTTPSession& self)
      : m_self(self)
    {}

    template<bool isRequest, class Body, class Fields>
    void operator()(http::message<isRequest, Body, Fields>&& msg) const
    {
      // The lifetime of the message has to extend
      // for the duration of the async operation so
      // we use a shared_ptr to manage it.
      auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(
        std::move(msg));

      // Store a type-erased version of the shared
      // pointer in the class to keep it alive.
      m_self.m_res = sp;

      // Write the response
      http::async_write(
        m_self.m_socket,
        *sp,
        boost::asio::bind_executor(m_self.m_strand,
                                   std::bind(&HTTPSession::onWrite,
                                             m_self.shared_from_this(),
                                             std::placeholders::_1,
                                             std::placeholders::_2,
                                             sp->need_eof())));
    }
  };

  Webserver* m_webserver;
  std::unique_ptr<Websocket> m_websocket;
  Logger m_logger;
  tcp::socket m_socket;
  boost::asio::strand<boost::asio::io_context::executor_type> m_strand;
  boost::beast::flat_buffer m_buffer;
  std::string const& m_docRoot;
  http::request<http::string_body> m_req;
  std::shared_ptr<void> m_res;
  send_lambda m_lambda;
};
class Webserver::HTTPListener
  : public std::enable_shared_from_this<Webserver::HTTPListener>
{
  public:
  HTTPListener(Webserver* webserver,
               asio::io_service& ioService,
               tcp::endpoint&& endpoint,
               std::string_view const& docRoot)
    : m_webserver(webserver)
    , m_logger(m_webserver->m_log->createLogger())
    , m_acceptor(ioService)
    , m_socket(ioService)
    , m_docRoot(docRoot)
  {
    boost::system::error_code ec;

    m_acceptor.open(endpoint.protocol(), ec);
    if(ec) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not open HTTP acceptor! Error: " << ec.message();
      return;
    }

    // Reuse, so connections can directly stay.
    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

    // Bind to the server address
    m_acceptor.bind(endpoint, ec);
    if(ec) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not bind HTTP acceptor! Error: " << ec.message();
      return;
    }

    // Start listening for connections
    m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if(ec) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not listen on HTTP acceptor! Error: " << ec.message();
      return;
    }
  }
  ~HTTPListener()
  {
    shutdown();
    PARACUBER_LOG(m_logger, Trace) << "Destruct HTTP Listener.";
  }

  void startAccept()
  {
    m_acceptor.async_accept(m_socket,
                            std::bind(&HTTPListener::onAccept,
                                      shared_from_this(),
                                      std::placeholders::_1));
  }

  void onAccept(boost::system::error_code ec)
  {
    if(ec) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not accept new connection on the HTTP port! Error: "
        << ec.message();
    } else {
      // Create the session and run it
      auto session = std::make_shared<HTTPSession>(
        m_webserver, std::move(m_socket), m_docRoot);
      m_sessions.push_back(session);
      session->run();

      // Accept another connection
      startAccept();
    }
  }

  void sessionClosed(HTTPSession* session)
  {
    std::remove_if(m_sessions.begin(),
                   m_sessions.end(),
                   std::bind(&removeIfCB, std::placeholders::_1, session));
  }

  void closeAllSessions()
  {
    for(auto& session : m_sessions) {
      if(!session.expired()) {
        auto ptr = session.lock();
        ptr->doClose();
      }
    }
    m_sessions.clear();
  }

  void shutdown()
  {
    closeAllSessions();
    m_acceptor.close();
  }

  void setWebserver(Webserver* server)
  {
    m_webserver = server;
    std::remove_if(m_sessions.begin(),
                   m_sessions.end(),
                   std::bind(&removeIfCB, std::placeholders::_1, nullptr));
    for(auto& session : m_sessions) {
      auto ptr = session.lock();
      ptr->setWebserver(server);
    }
  }

  static bool removeIfCB(const std::weak_ptr<HTTPSession> s, HTTPSession* match)
  {
    if(s.expired())
      return true;
    auto ptr = s.lock();
    return ptr.get() == match;
  }

  private:
  Webserver* m_webserver;
  Logger m_logger;
  tcp::acceptor m_acceptor;
  tcp::socket m_socket;
  std::string m_docRoot;
  std::vector<std::weak_ptr<HTTPSession>> m_sessions;
};

Webserver::Webserver(ConfigPtr config,
                     LogPtr log,
                     boost::asio::io_service& ioService)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
  , m_ioService(ioService)
  , m_api(std::make_unique<API>(this, m_config, m_log))
{
  PARACUBER_LOG(m_logger, Trace)
    << "Creating internal webserver. Reachable over port "
    << std::to_string(config->getUint16(Config::HTTPListenPort))
    << " with doc-root \"" << config->getString(Config::HTTPDocRoot)
    << "\". Link: " << buildLink();
}
Webserver::~Webserver()
{
  if(m_httpListener) {
    m_httpListener->setWebserver(nullptr);
  }
  PARACUBER_LOG(m_logger, Trace) << "Destruct Webserver.";
}

void
Webserver::run()
{
  if(!m_httpListener) {
    m_httpListener = std::make_shared<HTTPListener>(
      this,
      m_ioService,
      tcp::endpoint{ boost::asio::ip::tcp::v4(),
                     m_config->getUint16(Config::HTTPListenPort) },
      m_config->getString(Config::HTTPDocRoot));
    m_httpListener->startAccept();
  }
}
void
Webserver::stop()
{
  if(m_httpListener) {
    m_httpListener->shutdown();
    m_httpListener.reset();
  }
}

std::string
Webserver::buildLink()
{
  return "http://127.0.0.1:" +
         std::to_string(m_config->getUint16(Config::HTTPListenPort)) + "/";
}

void
Webserver::httpSessionClosed(HTTPSession* session)
{
  if(m_httpListener) {
    m_httpListener->sessionClosed(session);
  }
}
}
}
