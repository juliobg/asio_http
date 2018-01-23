/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/http_client.h"

#include "asio_http/internal/request_data.h"
#include "asio_http/internal/request_manager.h"

#include <boost/asio.hpp>
#include <curl/curl.h>
#include <memory>
#include <utility>
#include <vector>

namespace asio_http
{
http_client::http_client(const http_client_settings& settings)
    : m_io_context(m_internal_io_context)
    , m_io_thread([this]() { io_context_thread(); })
    , m_request_manager(std::make_shared<internal::request_manager>(settings, m_io_context))
{
}

http_client::http_client(const http_client_settings& settings, boost::asio::io_context& io_context)
    : m_io_context(io_context)
    , m_request_manager(std::make_shared<internal::request_manager>(settings, m_io_context))
{
}

http_client::~http_client()
{
  if (m_io_thread.joinable())
  {
    shut_down_io_context();
  }
}

void http_client::shut_down_io_context()
{
  m_io_context.stop();
  m_io_thread.join();
}

void http_client::io_context_thread()
{
  auto work = boost::asio::make_work_guard(m_io_context.get_executor());

  boost::system::error_code error_code;
  m_io_context.run(error_code);
}

void http_client::cancel_requests(const std::string& cancellation_token)
{
  m_request_manager->get_strand().post(
    [ ptr = m_request_manager, cancellation_token ]() { ptr->cancel_requests(cancellation_token); });
}
}  // namespace asio_http
