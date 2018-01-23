/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_LOGGING_FUNCTIONS_H
#define ASIO_HTTP_LOGGING_FUNCTIONS_H

#include <asio_http/http_request_result.h>

#include <chrono>
#include <curl/curl.h>

namespace asio_http
{
class http_request_result;
namespace internal
{
class curl_easy;
int                curl_debug_logging(CURL* unused, curl_infotype info_type, char* data, size_t size, void* url);
void               http_request_stats_logging(const http_request_result& result);
http_request_stats get_request_stats(curl_easy* ceasy, std::chrono::steady_clock::time_point creation_time);
}
}
#endif
