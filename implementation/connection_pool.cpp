/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/
#include "asio_http/internal/connection_pool.h"

#include "asio_http/internal/http_client_connection.h"
#include "asio_http/internal/http_content.h"
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

  auto* http_layer         = new http_client_connection<http_content>(shared_data, host);
  auto* http_content_layer = new http_content(shared_data, m_context);

  http_layer->upper_layer         = http_content_layer;
  http_content_layer->lower_layer = http_layer;

  if (url.protocol == "https")
  {
    auto transport = new ssl_socket<boost::asio::strand<boost::asio::io_context::executor_type>>(
      shared_data, m_context, url.host, ssl);
    http_layer->lower_layer = transport;
    transport->upper_layer  = http_layer;

    return http_stack(http_content_layer, http_layer, transport);
  }
  else
  {
    auto transport =
      new tcp_socket<boost::asio::strand<boost::asio::io_context::executor_type>>(shared_data, m_context);
    http_layer->lower_layer = transport;
    transport->upper_layer  = http_layer;

    return http_stack(http_content_layer, http_layer, transport);
  }
}

void connection_pool::release_connection(http_stack handle, bool clean_up)
{
  auto http_layer = handle.get<1>();

  const auto host = http_layer->get_host_and_port();

  // Throw away handle in case of error and clean all others
  if (clean_up)
  {
    auto new_stack = std::stack<http_stack>();
    m_connection_pool[host].swap(new_stack);
  }
  else
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
