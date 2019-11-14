/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_SOCKET_H
#define ASIO_HTTP_SOCKET_H

#include "asio_http/http_request.h"
#include "asio_http/internal/http_stack_shared.h"
#include "asio_http/internal/tuple_ptr.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>
#include <vector>

namespace asio_http
{
namespace internal
{
class transport_layer
{
public:
  transport_layer()
      : upper_layer(nullptr)
      , lower_layer(nullptr)
  {
  }
  virtual ~transport_layer() {}
  virtual void connect(const std::string&, const std::string&) {}
  virtual void on_connected(const boost::system::error_code&) {}
  virtual void read() {}
  virtual void on_read(const std::uint8_t*, std::size_t, boost::system::error_code) {}
  virtual void write(std::vector<std::uint8_t>) {}
  virtual void on_write(const boost::system::error_code&) {}
  virtual void close() {}
  virtual bool is_open() { return false; }
  void         set_upper(transport_layer* upper) { upper_layer = upper; }
  void         set_lower(transport_layer* lower) { lower_layer = lower; }

protected:
  transport_layer* upper_layer;
  transport_layer* lower_layer;

  void call_on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_read(data, size, ec);
    }
  }
  void call_on_connected(boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_connected(ec);
    }
  }
  void call_on_write(boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_write(ec);
    }
  }
};

template<typename Socket, typename Executor>
class generic_stream
    : public transport_layer
    , public shared_tuple_base<generic_stream<Socket, Executor>>
{
public:
  generic_stream(std::shared_ptr<http_stack_shared> shared_data, boost::asio::io_context& context)
      : transport_layer()
      , m_shared_data(shared_data)
      , m_read_buffer(1024)
      , m_socket(context)
      , m_resolver(context)
      , m_executor(shared_data->strand)
  {
  }

  virtual void connect(const std::string& host, const std::string& port) override
  {
    boost::asio::ip::tcp::resolver::query q(host, port);

    m_resolver.async_resolve(
      q, boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this()](auto&& ec, auto&& it) {
        ptr->resolve_handler(ec, it);
      }));
  }

  virtual bool is_open() override { return m_socket.is_open(); }

  virtual void write(std::vector<std::uint8_t> data) override
  {
    std::swap(m_write_buffer, data);
    boost::asio::async_write(
      m_socket,
      boost::asio::const_buffer(m_write_buffer.data(), m_write_buffer.size()),
      boost::asio::bind_executor(
        m_executor, [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->write_handler(ec, bytes); }));
  }

  virtual void read() override
  {
    m_socket.async_read_some(
      boost::asio::buffer(m_read_buffer),
      boost::asio::bind_executor(
        m_executor, [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->read_handler(ec, bytes); }));
  }

  virtual void close() override { m_socket.close(); }

private:
  std::shared_ptr<http_stack_shared> m_shared_data;
  Socket                             m_socket;
  std::vector<std::uint8_t>          m_write_buffer;
  std::vector<std::uint8_t>          m_read_buffer;
  boost::asio::ip::tcp::resolver     m_resolver;
  Executor                           m_executor;

  void resolve_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (ec)
    {
      call_on_connected(ec);
      return;
    }
    boost::asio::ip::tcp::endpoint ep = *it++;

    m_socket.async_connect(ep, boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this(), it](auto&& ec) {
                             ptr->connect_handler(ec, it);
                           }));
  }

  void connect_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (!ec || it == boost::asio::ip::tcp::resolver::iterator())
    {
      call_on_connected(ec);
    }
    else
    {
      boost::asio::ip::tcp::endpoint ep = *it++;
      m_socket.async_connect(ep,
                             boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this(), it](auto&& ec) {
                               ptr->connect_handler(ec, it);
                             }));
    }
  }

  void write_handler(const boost::system::error_code& ec, std::size_t bytes_transferred) { call_on_write(ec); }

  void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
  {
    call_on_read(m_read_buffer.data(), bytes_transferred, ec);
  }
};

template<typename Executor>
using tcp_socket = generic_stream<boost::asio::ip::tcp::socket, Executor>;

template<typename Executor>
class ssl_socket
    : public transport_layer
    , public shared_tuple_base<ssl_socket<Executor>>
{
public:
  ssl_socket(std::shared_ptr<http_stack_shared> shared_data,
             boost::asio::io_context&           context,
             const std::string&                 host,
             const ssl_settings&                ssl)
      : transport_layer()
      , m_shared_data(shared_data)
      , m_context(boost::asio::ssl::context::sslv23)
      , m_socket(context, m_context)
      , m_read_buffer(1024)
      , m_resolver(context)
      , m_executor(shared_data->strand)
  {
    m_context.set_default_verify_paths();
    if (!ssl.client_certificate_file.empty())
    {
      m_context.use_certificate_file(ssl.client_certificate_file, boost::asio::ssl::context_base::pem);
    }
    if (!ssl.client_private_key_file.empty())
    {
      m_context.use_private_key_file(ssl.client_private_key_file, boost::asio::ssl::context_base::pem);
    }
    if (!ssl.certificate_authority_bundle_file.empty())
    {
      m_context.use_certificate_chain_file(ssl.certificate_authority_bundle_file);
    }
    m_socket.set_verify_mode(boost::asio::ssl::verify_peer);
    m_socket.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
  }

  virtual void connect(const std::string& host, const std::string& port) override
  {
    boost::asio::ip::tcp::resolver::query q(host, port);

    m_resolver.async_resolve(
      q, boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this()](auto&& ec, auto&& it) {
        ptr->resolve_handler(ec, it);
      }));
  }

  virtual bool is_open() override { return m_socket.lowest_layer().is_open(); }

  virtual void write(std::vector<std::uint8_t> data) override
  {
    std::swap(m_write_buffer, data);
    boost::asio::async_write(
      m_socket,
      boost::asio::const_buffer(m_write_buffer.data(), m_write_buffer.size()),
      boost::asio::bind_executor(
        m_executor, [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->write_handler(ec, bytes); }));
  }

  virtual void read() override
  {
    m_socket.async_read_some(
      boost::asio::buffer(m_read_buffer),
      boost::asio::bind_executor(
        m_executor, [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->read_handler(ec, bytes); }));
  }

  virtual void close() override { m_socket.lowest_layer().close(); }

private:
  std::shared_ptr<http_stack_shared>                     m_shared_data;
  boost::asio::ssl::context                              m_context;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;
  std::vector<std::uint8_t>                              m_write_buffer;
  std::vector<std::uint8_t>                              m_read_buffer;
  boost::asio::ip::tcp::resolver                         m_resolver;
  Executor&                                              m_executor;

  void resolve_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (ec)
    {
      call_on_connected(ec);
      return;
    }
    boost::asio::ip::tcp::endpoint ep = *it++;

    m_socket.lowest_layer().async_connect(
      ep, boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this(), it](auto&& ec) {
        ptr->connect_handler(ec, it);
      }));
  }

  void connect_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (!ec)
    {
      m_socket.async_handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::handshake_type::client,
                               boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this()](auto&& ec) {
                                 ptr->handshake_handler(ec);
                               }));
    }
    else if (it != boost::asio::ip::tcp::resolver::iterator())
    {
      boost::asio::ip::tcp::endpoint ep = *it++;
      m_socket.lowest_layer().async_connect(
        ep, boost::asio::bind_executor(m_executor, [ptr = this->shared_from_this(), it](auto&& ec) {
          ptr->connect_handler(ec, it);
        }));
    }
    else
    {
      call_on_connected(ec);
    }
  }

  void handshake_handler(const boost::system::error_code& ec) { call_on_connected(ec); }

  void write_handler(const boost::system::error_code& ec, std::size_t) { call_on_write(ec); }

  void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
  {
    call_on_read(m_read_buffer.data(), bytes_transferred, ec);
  }
};
}  // namespace internal
}  // namespace asio_http

#endif
