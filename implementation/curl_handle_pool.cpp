/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/curl_handle_pool.h"

#include "asio_http/internal/curl_easy.h"

namespace asio_http
{
namespace internal
{
std::shared_ptr<curl_easy> curl_handle_pool::get_curl_handle(const std::pair<std::string, uint16_t>& host)
{
  std::shared_ptr<curl_easy> handle;

  if (m_curl_handle_pool[host].empty())
  {
    handle = std::make_shared<curl_easy>(host, false);
  }
  else
  {
    handle = m_curl_handle_pool[host].top();
    m_curl_handle_pool[host].pop();
  }

  return handle;
}

void curl_handle_pool::release_handle(std::shared_ptr<curl_easy> handle, bool clean_up)
{
  const auto host = handle->get_host_and_port();

  // Throw away handle in case of error, and create a new one to "replace" it
  if (clean_up)
  {
    m_curl_handle_pool[host].push(std::make_shared<curl_easy>(host, true));
  }
  else
  {
    m_curl_handle_pool[host].push(handle);
  }
}
}
}
