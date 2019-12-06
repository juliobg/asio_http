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
#include "asio_http/internal/http_stack_shared.h"
#include "asio_http/internal/socket.h"
#include "asio_http/internal/tuple_ptr.h"

#include "http_parser.h"

#include <algorithm>
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

struct request_buffers
{
  request_buffers(http_method method_, std::vector<std::pair<std::string, std::string>> request_headers_, url url)
      : method(method_)
      , request_headers(std::move(request_headers_))
      , m_url(std::move(url))
      , m_state(connection_state::in_progress)
  {
  }
  request_buffers()        = default;
  request_buffers& operator=(request_buffers&&) = default;

  http_method                                      method;
  std::vector<std::pair<std::string, std::string>> request_headers;
  std::vector<std::pair<std::string, std::string>> headers;
  url                                              m_url;
  std::pair<std::string, std::string>              m_current_header;
  connection_state                                 m_state;

  void push_current_header()
  {
    // Trim trailing whitespaces
    m_current_header.second.erase(m_current_header.second.find_last_not_of("\n\r\t ") + 1);
    headers.push_back(std::move(m_current_header));
    m_current_header.first.clear();
    m_current_header.second.clear();
  }

  std::vector<std::uint8_t> print_request_headers()
  {
    std::stringstream request_headers_string;
    request_headers_string << http_method_to_string(method) << " " << m_url.path << " HTTP/1.1\r\n";

    for (const auto& header : request_headers)
    {
      request_headers_string << header.first << ": " << header.second << "\r\n";
    }
    request_headers_string << "\r\n";

    const std::string headers = request_headers_string.str();

    return std::vector<std::uint8_t>(headers.begin(), headers.end());
  }
};

template<std::size_t N, typename Ls>
class http_client_connection
    : public protocol_layer
    , public shared_tuple_base<http_client_connection<N, Ls>>
{
public:
  http_client_connection(std::shared_ptr<http_stack_shared> shared_data);
  ~http_client_connection() override {}

  virtual void on_connected(const boost::system::error_code& ec) override;
  virtual void on_write(const boost::system::error_code& ec) override;
  virtual void on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec) override;
  typename Ls::template type<N - 1>*    upper_layer;
  typename Ls::template type<N + 1>*    lower_layer;

  void write_headers(http_method method, url url, std::vector<std::pair<std::string, std::string>> headers);
  void close() override { lower_layer->close(); }

private:
  void       write_headers_read();
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

  std::shared_ptr<http_stack_shared>                           m_shared_data;
  boost::asio::strand<boost::asio::io_context::executor_type>& m_strand;

  http_parser_settings                  m_settings;
  http_parser                           m_parser;

  request_buffers m_current_request;

  bool m_not_reusable;
};

template<std::size_t N, typename Ls>
inline http_client_connection<N, Ls>::http_client_connection(std::shared_ptr<http_stack_shared>    shared_data)
    : m_shared_data(shared_data)
    , m_strand(shared_data->strand)
    , m_settings()
    , m_parser()
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

template<std::size_t N, typename Ls>
inline void http_client_connection<N, Ls>::write_headers(http_method                              method,
                                                 url                                              url,
                                                 std::vector<std::pair<std::string, std::string>> headers)
{
  m_current_request         = request_buffers(method, std::move(headers), std::move(url));
  m_current_request.m_state = connection_state::in_progress;

  if (lower_layer->is_open())
  {
    send_headers();
  }
  else
  {
    lower_layer->connect(m_current_request.m_url.host, std::to_string(m_current_request.m_url.port));
  }
}

template<std::size_t N, typename Ls>
inline void http_client_connection<N, Ls>::send_headers()
{
  lower_layer->write(m_current_request.print_request_headers());
}

template<std::size_t N, typename Ls>
inline void http_client_connection<N, Ls>::on_connected(const boost::system::error_code& ec)
{
  if (!ec)
  {
    send_headers();
  }
  else
  {
    upper_layer->on_error(ec);
  }
}
template<std::size_t N, typename Ls>
inline void http_client_connection<N, Ls>::on_write(const boost::system::error_code& ec)
{
  if (ec)
  {
    upper_layer->on_error(ec);
  }
  std::vector<std::uint8_t> buf(1024);
  auto                      count = upper_layer->get_body_data(reinterpret_cast<char*>(buf.data()), buf.size());
  if (count == 0)
  {
    lower_layer->read();
  }
  else
  {
    buf.resize(count);
    lower_layer->write(buf);
  }
}

template<std::size_t N, typename Ls>
inline void
http_client_connection<N, Ls>::on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec)
{
  if (ec)
  {
    m_not_reusable = true;
  }

  std::size_t nsize;

  // If there is available data, try to parse response, and ignore errors
  if (size != 0 && m_current_request.m_state != connection_state::done)
  {
    const char* d = reinterpret_cast<const char*>(data);
    nsize         = http_parser_execute(&m_parser, &m_settings, d, size);
  }

  // connection_state may have changed after call to http_parser_execute
  if (m_current_request.m_state != connection_state::done)
  {
    if (nsize != size && size != 0)
    {
      upper_layer->on_error(HTTP_PARSER_ERRNO(&m_parser));
    }
    else if (!ec)
    {
      lower_layer->read();
    }
    else
    {
      upper_layer->on_error(ec);
    }
  }
}

template<std::size_t N, typename Ls>
inline int http_client_connection<N, Ls>::on_body(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  obj->upper_layer->on_body(at, length);
  return 0;
}

template<std::size_t N, typename Ls>
inline int http_client_connection<N, Ls>::on_message_complete(http_parser* parser)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  if (http_should_keep_alive(parser) == 0)
  {
    obj->m_not_reusable = true;
  }
  obj->m_current_request.m_state = connection_state::done;
  obj->upper_layer->message_complete();

  return 0;
}

template<std::size_t N, typename Ls>
inline int http_client_connection<N, Ls>::on_headers_complete(http_parser* parser)
{
  http_client_connection* obj = static_cast<http_client_connection*>(parser->data);
  if (!obj->m_current_request.m_current_header.first.empty())
  {
    obj->m_current_request.push_current_header();
  }

  obj->upper_layer->on_headers(parser->status_code, obj->m_current_request.headers);

  if (obj->m_current_request.method == http_method::HEAD)
  {
    return 1;
  }
  return 0;
}
template<std::size_t N, typename Ls>
inline int http_client_connection<N, Ls>::on_status(http_parser*, const char*, size_t)
{
  return 0;
}

template<std::size_t N, typename Ls>
inline int http_client_connection<N, Ls>::on_header_field(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj     = static_cast<http_client_connection*>(parser->data);
  auto&                   current = obj->m_current_request.m_current_header;
  if (!current.second.empty())
  {
    obj->m_current_request.push_current_header();
  }
  current.first.append(at, length);
  return 0;
}

template<std::size_t N, typename Ls>
inline int http_client_connection<N, Ls>::on_header_value(http_parser* parser, const char* at, size_t length)
{
  http_client_connection* obj     = static_cast<http_client_connection*>(parser->data);
  auto&                   current = obj->m_current_request.m_current_header;
  current.second.append(at, length);
  if (current.second.empty())
  {
    obj->m_current_request.push_current_header();
  }
  return 0;
}
}  // namespace internal
}  // namespace asio_http

#endif
