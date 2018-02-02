/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CLIENT_CONNECTION_H
#define ASIO_HTTP_CLIENT_CONNECTION_H

#include <asio_http/http_request_interface.h>
#include <asio_http/http_request_result.h>
#include <asio_http/internal/data_sink.h>
#include <asio_http/internal/data_source.h>
#include <asio_http/internal/error_categories.h>

#include "http_parser.h"

#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace asio_http
{
namespace internal
{
enum class connection_state
{
  in_progress,
  done
};

inline std::string http_method_to_string(http_request_interface::http_method method)
{
  static const std::map<http_request_interface::http_method, std::string> map{
    { http_request_interface::http_method::GET, "GET" },
    { http_request_interface::http_method::HEAD, "HEAD" },
    { http_request_interface::http_method::PUT, "PUT" },
    { http_request_interface::http_method::POST, "POST" }
  };

  return map.at(method);
}

struct request_buffers
{
  request_buffers(std::shared_ptr<const http_request_interface> request)
      : m_request(request)
      , m_body_buffer(1024)
      , m_data_source(request->get_post_data(), request->get_compress_post_data_policy())
      , m_state(connection_state::in_progress)
  {
  }
  boost::asio::streambuf                        request_buffer_;
  std::shared_ptr<const http_request_interface> m_request;
  std::vector<std::string>                      m_headers;
  std::pair<std::string, std::string>           m_current_header;
  std::vector<uint8_t>                          m_body_buffer;
  data_sink                                     m_data_sink;
  data_source                                   m_data_source;
  connection_state                              m_state;

  void push_current_header()
  {
    std::string header = m_current_header.first + ": " + m_current_header.second;
    // Trim trailing whitespaces
    header.erase(header.find_last_not_of("\n\r\t ") + 1);
    m_headers.push_back(header);
    m_current_header.first.clear();
    m_current_header.second.clear();
  }

  void print_request_headers()
  {
    std::ostream request_headers(&request_buffer_);
    request_headers << http_method_to_string(m_request->get_http_method()) << " " << m_request->get_url().path
                    << " HTTP/1.1\r\n";

    request_headers << "Host: " << m_request->get_url().host << "\r\n";
    for (const auto& header : m_request->get_http_headers())
    {
      request_headers << header << "\r\n";
    }
    if (m_data_source.get_size() != 0)
    {
      request_headers << "Content-Length: " << m_data_source.get_size() << "\r\n";
    }
    request_headers << "\r\n";
  }
};

struct http_client_connection : std::enable_shared_from_this<http_client_connection>
{
  typedef std::shared_ptr<http_client_connection> pointer;
  http_client_connection(boost::asio::io_context::strand& strand, std::pair<std::string, uint16_t> host);
  ~http_client_connection() {}
  void       start(std::shared_ptr<const http_request_interface>                                           request,
                   std::function<void(std::shared_ptr<http_client_connection>, boost::system::error_code)> callback);
  void       resolve_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator i);
  void       connect_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator i);
  void       start_read();
  void       write_handler(const boost::system::error_code& error, std::size_t bytes_transferred);
  void       write_body_handler(const boost::system::error_code& error, std::size_t bytes_transferred);
  void       read_handler(const boost::system::error_code& error, std::size_t bytes_transferred);
  static int on_header_field(http_parser* parser, const char* at, size_t length);
  static int on_header_value(http_parser* parser, const char* at, size_t length);
  static int on_body(http_parser* parser, const char* at, size_t length);
  static int on_message_complete(http_parser* parser);
  static int on_headers_complete(http_parser* parser);
  static int on_status(http_parser* parser, const char* at, size_t length);
  void       write_body();
  void       send_headers();
  boost::asio::io_context::strand& m_strand;
  boost::asio::io_context&         io_service_;
  boost::asio::ip::tcp::resolver   resolver_;
  boost::asio::ip::tcp::socket     socket_;

  boost::asio::streambuf                                                                  response_buffer_;
  http_parser_settings                                                                    settings_;
  http_parser                                                                             parser_;
  std::function<void(std::shared_ptr<http_client_connection>, boost::system::error_code)> m_completed_request_callback;
  std::pair<std::string, uint16_t>                                                        m_host_port;

  std::unique_ptr<request_buffers> m_current_request;

  boost::asio::deadline_timer m_timer;

  std::string  status_;
  unsigned int get_response_code() const
  {
    assert(parser_.type == HTTP_RESPONSE);
    return parser_.status_code;
  }

  std::pair<std::string, uint16_t> get_host_and_port() const { return m_host_port; }

  std::vector<uint8_t> get_data() const
  {
    return m_current_request ? m_current_request->m_data_sink.get_data() : std::vector<uint8_t>();
  }

  std::vector<std::string> get_reply_headers() const { return m_current_request->m_headers; }

  void cancel()
  {
    socket_.close();
    complete_request(boost::asio::error::operation_aborted);
  }

  void complete_request(boost::system::error_code ec, bool close = false)
  {
    if ((ec || close) && socket_.is_open())
    {
      socket_.close();
    }
    if (m_current_request->m_state != connection_state::done)
    {
      m_current_request->m_state = connection_state::done;
      m_timer.cancel();
      m_completed_request_callback(shared_from_this(), ec);
    }
  }

  bool is_open() { return socket_.is_open(); }
};

inline http_client_connection::http_client_connection(boost::asio::io_context::strand& strand,
                                                      std::pair<std::string, uint16_t> host)
    : m_strand(strand)
    , io_service_(strand.context())
    , resolver_(io_service_)
    , socket_(io_service_)
    , settings_()
    , parser_()
    , m_host_port(host)
    , m_timer(io_service_)
{
  http_parser_init(&parser_, HTTP_RESPONSE);
  parser_.data                  = this;
  settings_.on_body             = &http_client_connection::on_body;
  settings_.on_message_complete = &http_client_connection::on_message_complete;
  settings_.on_status           = &http_client_connection::on_status;
  settings_.on_header_field     = &http_client_connection::on_header_field;
  settings_.on_header_value     = &http_client_connection::on_header_value;
  settings_.on_headers_complete = &http_client_connection::on_headers_complete;
}

inline void http_client_connection::start(
  std::shared_ptr<const http_request_interface>                                           request,
  std::function<void(std::shared_ptr<http_client_connection>, boost::system::error_code)> callback)
{
  m_current_request.reset(new request_buffers(request));
  m_completed_request_callback = callback;

  m_timer.expires_from_now(boost::posix_time::millisec(request->get_timeout_msec()));
  m_timer.async_wait([ptr = shared_from_this()](auto&& ec) {
    if (!ec)
    {
      ptr->complete_request(boost::asio::error::timed_out);
      ptr->socket_.close();
    }
  });

  if (socket_.is_open())
  {
    send_headers();
  }
  else
  {
    boost::asio::ip::tcp::resolver::query q(request->get_url().host, std::to_string(request->get_url().port));

    resolver_.async_resolve(q, [ptr = shared_from_this()](auto&& ec, auto&& it) { ptr->resolve_handler(ec, it); });
  }
}

inline void http_client_connection::resolve_handler(const boost::system::error_code&         ec,
                                                    boost::asio::ip::tcp::resolver::iterator i)
{
  if (ec)
  {
    complete_request(ec);
    return;
  }

  boost::asio::ip::tcp::endpoint ep = *i;
  socket_.async_connect(ep, [ ptr = shared_from_this(), i ](auto&& ec) { ptr->connect_handler(ec, i); });
}

inline void http_client_connection::send_headers()
{
  m_current_request->print_request_headers();

  boost::asio::async_write(
    socket_, m_current_request->request_buffer_, [ptr = shared_from_this()](auto&& ec, auto&& bytes) {
      ptr->write_handler(ec, bytes);
    });
}

inline void http_client_connection::connect_handler(const boost::system::error_code&         ec,
                                                    boost::asio::ip::tcp::resolver::iterator i)
{
  if (!ec)
  {
    send_headers();
  }
  // Retry to connect if error
  else if (i != boost::asio::ip::tcp::resolver::iterator())
  {
    boost::asio::ip::tcp::endpoint ep = *i;
    socket_.async_connect(ep, [ ptr = shared_from_this(), i ](auto&& ec) { ptr->connect_handler(ec, i); });
  }
  else
  {
    complete_request(ec);
  }
}

inline void http_client_connection::start_read()
{
  socket_.async_read_some(response_buffer_.prepare(1024), [ptr = shared_from_this()](auto&& ec, auto&& bytes) {
    ptr->read_handler(ec, bytes);
  });
}

inline void http_client_connection::write_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
  if (ec)
  {
    complete_request(ec);
    return;
  }
  m_current_request->request_buffer_.consume(bytes_transferred);
  if (m_current_request->m_data_source.get_size() == 0)
  {
    start_read();
  }
  else
  {
    write_body();
  }
}

inline void http_client_connection::write_body()
{
  auto count = m_current_request->m_data_source.read_callback(
    reinterpret_cast<char*>(m_current_request->m_body_buffer.data()), m_current_request->m_body_buffer.size());

  if (count != 0)
  {
    boost::asio::async_write(
      socket_,
      boost::asio::const_buffer(m_current_request->m_body_buffer.data(), count),
      [ptr = shared_from_this()](auto&& ec, auto&& bytes) { ptr->write_body_handler(ec, bytes); });
  }
  else
  {
    start_read();
  }
}

inline void http_client_connection::write_body_handler(const boost::system::error_code& error,
                                                       std::size_t                      bytes_transferred)
{
  if (!error && bytes_transferred)
  {
    write_body();
  }
  else
  {
    complete_request(error);
  }
}

inline void http_client_connection::read_handler(const boost::system::error_code& error, std::size_t bytes_transferred)
{
  if ((!error && bytes_transferred) || error == boost::asio::error::eof)
  {
    const char* data = boost::asio::buffer_cast<const char*>(response_buffer_.data());

    std::size_t nsize = http_parser_execute(&parser_, &settings_, data, bytes_transferred);
    if (nsize != bytes_transferred)
    {
      complete_request(HTTP_PARSER_ERRNO(&parser_));
      return;
    }
    response_buffer_.consume(nsize);
    if (m_current_request->m_state != connection_state::done)
    {
      start_read();
    }
  }
  else
  {
    complete_request(error);
  }
}

inline int http_client_connection::on_body(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  obj->m_current_request->m_data_sink.write_callback(at, length, 1);
  return 0;
}

inline int http_client_connection::on_message_complete(http_parser* parser)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  obj->complete_request(boost::system::error_code(), http_should_keep_alive(parser) == 0);

  return 0;
}

inline int http_client_connection::on_headers_complete(http_parser* parser)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  if (!obj->m_current_request->m_current_header.first.empty())
  {
    obj->m_current_request->push_current_header();
  }
  if (obj->m_current_request->m_request->get_http_method() == http_request_interface::http_method::HEAD)
  {
    return 1;
  }
  return 0;
}

inline int http_client_connection::on_status(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  obj->status_.append(at, length);
  return 0;
}

inline int http_client_connection::on_header_field(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj     = static_cast<http_client_connection*>(parser->data);
  auto&                   current = obj->m_current_request->m_current_header;
  if (!current.second.empty())
  {
    obj->m_current_request->push_current_header();
  }
  current.first.append(at, length);
  return 0;
}

inline int http_client_connection::on_header_value(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj     = static_cast<http_client_connection*>(parser->data);
  auto&                   current = obj->m_current_request->m_current_header;
  current.second.append(at, length);
  if (current.second.empty())
  {
    obj->m_current_request->push_current_header();
  }
  return 0;
}
}
}

#endif
