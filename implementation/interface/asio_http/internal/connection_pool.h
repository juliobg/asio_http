/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CONNECTION_POOL_H
#define ASIO_HTTP_CONNECTION_POOL_H

#include <asio_http/internal/tuple_ptr.h>

#include <boost/asio.hpp>

#include <map>
#include <memory>
#include <stack>

namespace asio_http
{
class url;
class ssl_settings;

namespace internal
{
template<std::size_t N, typename Ls>
class http_client_connection;
class protocol_layer;
template<std::size_t N, typename Ls>
class http_content;
struct http_stack_interface;

using http_stack = tuple_element_ptr<http_stack_interface>;

class connection_pool
{
public:
  connection_pool(boost::asio::io_context& context)
      : m_context(context)
      , m_allocations(0)
  {
  }
  ~connection_pool();
  http_stack get_connection(const url& url, const ssl_settings& ssl);
  void       release_connection(http_stack handle, bool clean_up);

private:
  http_stack create_stack(const url& url, const ssl_settings& ssl);

  boost::asio::io_context&                                                m_context;
  std::map<std::pair<std::string, std::uint16_t>, std::stack<http_stack>> m_connection_pool;
  uint64_t                                                                m_allocations;
};
}  // namespace internal
}  // namespace asio_http
#endif
