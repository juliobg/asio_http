/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/completion_handler_invoker.h"

#include "asio_http/http_request_result.h"
#include "asio_http/internal/request_data.h"

#include <boost/asio.hpp>
#include <thread>

namespace asio_http
{
namespace internal
{
void completion_handler_invoker::invoke_handler(const request_data& request_data, http_request_result result)
{
  boost::asio::dispatch(
    request_data.m_completion_executor,
    [res = std::move(result), handler = request_data.m_completion_handler]() mutable { handler(std::move(res)); });
}
}  // namespace internal
}  // namespace asio_http
