/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/http_client.h"

#include "asio_http/internal/request_data.h"
#include "asio_http/internal/request_manager.h"

#include <boost/asio.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace asio_http
{
http_client::http_client(const http_client_settings& settings, boost::asio::io_context& io_context)
    : m_io_context(io_context)
    , m_request_manager(std::make_shared<internal::request_manager>(settings, m_io_context))
{
}

http_client::~http_client()
{
  m_request_manager->cancel_requests_async({});
}

void http_client::cancel_requests(std::string cancellation_token)
{
  m_request_manager->cancel_requests_async(std::move(cancellation_token));
}
}  // namespace asio_http
