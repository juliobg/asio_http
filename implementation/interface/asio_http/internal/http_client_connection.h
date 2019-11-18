/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CLIENT_CONNECTION_H
#define ASIO_HTTP_CLIENT_CONNECTION_H

#include "asio_http/error.h"
#include "asio_http/http_request.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/connection_pool.h"
#include "asio_http/internal/data_sink.h"
#include "asio_http/internal/data_source.h"
#include "asio_http/internal/http_stack_shared.h"
#include "asio_http/internal/socket.h"
#include "asio_http/internal/tuple_ptr.h"

#include "http_parser.h"

#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace asio_http
{
namespace internal
{
enum class connection_state
{
  in_progress,
  writing_body,
  done
};
inline std::string http_method_to_string(http_method method)
{
  static const std::map<http_method, std::string> map{ { http_method::GET, "GET" },
                                                       { http_method::HEAD, "HEAD" },
                                                       { http_method::PUT, "PUT" },
                                                       { http_method::POST, "POST" } };

  return map.at(method);
}

struct http_result_data
{
  std::shared_ptr<const http_request>              m_request;
  std::vector<std::pair<std::string, std::string>> m_headers;
  unsigned int                                     m_status_code;
  std::vector<uint8_t>                             data;
};

struct request_buffers
{
  request_buffers(std::shared_ptr<const http_request> request)
      : m_request(request)
      , m_data_source(request->get_post_data(), request->get_compress_post_data_policy())
      , m_state(connection_state::in_progress)
  {
  }
  std::shared_ptr<const http_request> m_request;
  std::pair<std::string, std::string> m_current_header;
  data_sink                           m_data_sink;
  data_source                         m_data_source;
  connection_state                    m_state;
  http_result_data                    m_result;

  void push_current_header()
  {
    // Trim trailing whitespaces
    m_current_header.second.erase(m_current_header.second.find_last_not_of("\n\r\t ") + 1);
    m_result.m_headers.push_back(std::move(m_current_header));
    m_current_header.first.clear();
    m_current_header.second.clear();
  }

  std::vector<std::uint8_t> print_request_headers()
  {
    std::stringstream request_headers;
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

    const std::string headers = request_headers.str();

    return std::vector<std::uint8_t>(headers.begin(), headers.end());
  }
};

class http_client_connection
    : public transport_layer
    , public shared_tuple_base<http_client_connection>
{
public:
  http_client_connection(std::shared_ptr<http_stack_shared>    shared_data,
                         boost::asio::io_context&              io_context,
                         std::pair<std::string, std::uint16_t> host);
  ~http_client_connection() override { lower_layer->set_upper(nullptr); }

  virtual void on_connected(const boost::system::error_code& ec) override;
  virtual void on_write(const boost::system::error_code& ec) override;
  virtual void on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec) override;
  void         start_async(std::shared_ptr<const http_request>                                request,
                           std::function<void(http_result_data&&, boost::system::error_code)> callback)
  {
    async<&http_client_connection::start>(std::move(request), std::move(callback));
  }
  std::pair<std::string, std::uint16_t> get_host_and_port() const { return m_host_port; }
  void                                  cancel_async() { async<&http_client_connection::cancel>(); }

private:
  // This is a work-around as we don't have C++20 lambdas perfect capture in C++17
  template<auto F, typename... Args>
  void async(Args&&... args)
  {
    boost::asio::post(
      m_strand, [ptr = shared_from_this(), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply([ptr = ptr.get()](auto&&... args) { (ptr->*F)(std::move(args)...); }, std::move(args));
      });
  }
  void       start(std::shared_ptr<const http_request>                                request,
                   std::function<void(http_result_data&&, boost::system::error_code)> callback);
  void       cancel() { complete_request(make_error_code(boost::asio::error::operation_aborted)); }
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
  std::shared_ptr<http_stack_shared> m_shared_data;
  boost::asio::io_context::strand&   m_strand;

  http_parser_settings                                               m_settings;
  http_parser                                                        m_parser;
  std::function<void(http_result_data&&, boost::system::error_code)> m_completed_request_callback;
  std::pair<std::string, std::uint16_t>                              m_host_port;

  std::unique_ptr<request_buffers> m_current_request;

  boost::asio::deadline_timer m_timer;
  bool                        m_not_reusable;

  void complete_request(boost::system::error_code ec)
  {
    if (ec || m_not_reusable)
    {
      m_not_reusable = true;
      lower_layer->close();
    }
    if (m_current_request->m_state != connection_state::done)
    {
      m_current_request->m_state = connection_state::done;
      m_timer.cancel();
      m_current_request->m_result.m_status_code = m_parser.status_code;
      m_current_request->m_result.data          = m_current_request->m_data_sink.get_data();
      m_current_request->m_result.m_request     = m_current_request->m_request;
      m_completed_request_callback(std::move(m_current_request->m_result), ec);
      m_current_request->m_result = {};  // After cancellation or timeout it may happen request still running
    }
    m_completed_request_callback = nullptr;
  }
};

inline http_client_connection::http_client_connection(std::shared_ptr<http_stack_shared>    shared_data,
                                                      boost::asio::io_context&              io_context,
                                                      std::pair<std::string, std::uint16_t> host)
    : m_shared_data(shared_data)
    , m_strand(shared_data->strand)
    , m_settings()
    , m_parser()
    , m_host_port(host)
    , m_timer(io_context)
    , m_not_reusable(false)
{
  http_parser_init(&m_parser, HTTP_RESPONSE);
  m_parser.data                  = this;
  m_settings.on_body             = &http_client_connection::on_body;
  m_settings.on_message_complete = &http_client_connection::on_message_complete;
  m_settings.on_status           = &http_client_connection::on_status;
  m_settings.on_header_field     = &http_client_connection::on_header_field;
  m_settings.on_header_value     = &http_client_connection::on_header_value;
  m_settings.on_headers_complete = &http_client_connection::on_headers_complete;
}

inline void http_client_connection::start(std::shared_ptr<const http_request>                                request,
                                          std::function<void(http_result_data&&, boost::system::error_code)> callback)
{
  m_current_request.reset(new request_buffers(request));
  m_completed_request_callback = callback;

  m_timer.expires_from_now(boost::posix_time::millisec(request->get_timeout_msec()));
  m_timer.async_wait([ptr = shared_from_this()](auto&& ec) {
    if (!ec)
    {
      ptr->complete_request(boost::asio::error::timed_out);
    }
  });

  if (lower_layer->is_open())
  {
    send_headers();
  }
  else
  {
    lower_layer->connect(request->get_url().host, std::to_string(request->get_url().port));
  }
}

inline void http_client_connection::send_headers()
{
  lower_layer->write(m_current_request->print_request_headers());
}

inline void http_client_connection::on_connected(const boost::system::error_code& ec)
{
  if (!ec)
  {
    send_headers();
  }
  else
  {
    complete_request(ec);
  }
}

inline void http_client_connection::on_write(const boost::system::error_code& ec)
{
  if (ec)
  {
    complete_request(ec);
  }
  else if (m_current_request->m_data_source.get_size() == 0)
  {
    lower_layer->read();
  }
  else
  {
    std::vector<std::uint8_t> buf(1024);
    auto count = m_current_request->m_data_source.read_callback(reinterpret_cast<char*>(buf.data()), buf.size());
    if (count != 0)
    {
      buf.resize(count);
      lower_layer->write(buf);
    }
    else
    {
      lower_layer->read();
    }
  }
}

inline void http_client_connection::on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec)
{
  if (ec)
  {
    m_not_reusable = true;
  }
  // If there is available data, try to parse response, and ignore errors
  if (size != 0 && m_current_request->m_state != connection_state::done)
  {
    const char* d     = reinterpret_cast<const char*>(data);
    std::size_t nsize = http_parser_execute(&m_parser, &m_settings, d, size);

    if (nsize != size)
    {
      complete_request(HTTP_PARSER_ERRNO(&m_parser));
    }
  }
  // connection_state may have changed after call to http_parser_execute
  if (!ec && m_current_request->m_state != connection_state::done)
  {
    lower_layer->read();
  }
  else if (ec && m_current_request->m_state != connection_state::done)
  {
    complete_request(ec);
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
  if (http_should_keep_alive(parser) == 0)
  {
    obj->m_not_reusable = true;
  }
  obj->complete_request(boost::system::error_code());

  return 0;
}

inline int http_client_connection::on_headers_complete(http_parser* parser)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  if (!obj->m_current_request->m_current_header.first.empty())
  {
    obj->m_current_request->push_current_header();
  }
  if (obj->m_current_request->m_request->get_http_method() == http_method::HEAD)
  {
    return 1;
  }
  return 0;
}

inline int http_client_connection::on_status(http_parser*, const char*, size_t)
{
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
}  // namespace internal
}  // namespace asio_http

#endif
