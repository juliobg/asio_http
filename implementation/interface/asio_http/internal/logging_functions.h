/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
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
class http_client_connection;
void               http_request_stats_logging(const http_request_result& result);
http_request_stats get_request_stats(http_client_connection*               connection,
                                     std::chrono::steady_clock::time_point creation_time);
}
}
#endif
