/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_LOGGING_FUNCTIONS_H
#define ASIO_HTTP_LOGGING_FUNCTIONS_H

#include <asio_http/http_request_result.h>

#include <chrono>

namespace asio_http
{
class http_request_result;
namespace internal
{
void               http_request_stats_logging(const http_request_result& result, const std::string& name);
http_request_stats get_request_stats(std::chrono::steady_clock::time_point creation_time);
}  // namespace internal
}  // namespace asio_http
#endif
