/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/connection_pool.h"

#include "asio_http/internal/http_client_connection.h"

namespace asio_http
{
namespace internal
{
std::shared_ptr<http_client_connection> connection_pool::get_connection(const std::pair<std::string, uint16_t>& host)
{
  std::shared_ptr<http_client_connection> handle;

  if (m_connection_pool[host].empty())
  {
    handle = std::make_shared<http_client_connection>(m_strand, host);
  }
  else
  {
    handle = m_connection_pool[host].top();
    m_connection_pool[host].pop();
  }

  return handle;
}

void connection_pool::release_connection(std::shared_ptr<http_client_connection> handle, bool clean_up)
{
  const auto host = handle->get_host_and_port();

  // Throw away handle in case of error and clean all others
  if (clean_up)
  {
    auto new_stack = std::stack<std::shared_ptr<http_client_connection>>();
    m_connection_pool[host].swap(new_stack);
  }
  else if (handle->is_open())
  {
    // m_connection_pool[host].push(handle);
    handle->socket_.close();
  }
}
}
}
