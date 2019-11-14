/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/
#include "asio_http/internal/connection_pool.h"

#include "asio_http/internal/http_client_connection.h"
#include "asio_http/internal/http_stack_shared.h"
#include "asio_http/internal/socket.h"
#include "asio_http/url.h"

#include "loguru.hpp"

#include <cinttypes>
#include <memory>
#include <utility>

namespace asio_http
{
namespace internal
{
http_stack connection_pool::get_connection(const url& url, const ssl_settings& ssl)
{
  http_stack handle;

  auto host = std::make_pair(url.host, url.port);

  if (m_connection_pool[host].empty())
  {
    handle = create_stack(url, ssl);
    m_allocations++;
  }
  else
  {
    handle = m_connection_pool[host].top();
    m_connection_pool[host].pop();
  }

  return handle;
}

http_stack connection_pool::create_stack(const url& url, const ssl_settings& ssl)
{
  // TODO This code is not exception safe
  auto host        = std::make_pair(url.host, url.port);
  auto shared_data = std::make_shared<http_stack_shared>(m_context);

  auto* http_layer = new http_client_connection(shared_data, m_context, host);

  if (url.protocol == "https")
  {
    auto transport = new ssl_socket<boost::asio::io_context::strand>(shared_data, m_context, url.host, ssl);
    http_layer->set_lower(transport);
    transport->set_upper(http_layer);

    return http_stack(http_layer, transport);
  }
  else
  {
    auto transport = new tcp_socket<boost::asio::io_context::strand>(shared_data, m_context);
    http_layer->set_lower(transport);
    transport->set_upper(http_layer);

    return http_stack(http_layer, transport);
  }
}

void connection_pool::release_connection(http_stack handle, bool clean_up)
{
  auto top = handle.get<0>();

  const auto host = top->get_host_and_port();

  // Throw away handle in case of error and clean all others
  if (clean_up)
  {
    auto new_stack = std::stack<http_stack>();
    m_connection_pool[host].swap(new_stack);
  }
  else if (top->is_valid_connection())
  {
    m_connection_pool[host].push(handle);
  }
}

connection_pool::~connection_pool()
{
  DLOG_F(INFO, "Destroyed connection pool after allocations: %" PRIu64, m_allocations);
}
}  // namespace internal
}  // namespace asio_http
