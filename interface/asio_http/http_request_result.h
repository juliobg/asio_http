/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_HTTP_REQUEST_RESULT_H
#define ASIO_HTTP_HTTP_REQUEST_RESULT_H

#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace asio_http
{
class http_request_interface;

struct http_request_stats
{
  std::chrono::duration<double> name_lookup_time_s;
  std::chrono::duration<double> total_time_s;
  int64_t                       avg_download_speed_bps;
  int64_t                       avg_upload_speed_bps;
  int64_t                       downloaded_bytes;
  int64_t                       uploaded_bytes;
};

class http_request_result
{
public:
  http_request_result(std::shared_ptr<const http_request_interface> request_,
                      uint32_t                                      http_response_code_,
                      std::vector<std::string>                      headers_,
                      std::vector<uint8_t>                          content_,
                      std::error_code                               error_,
                      http_request_stats                            request_stats_)
      : request(std::move(request_))
      , http_response_code(http_response_code_)
      , headers(std::move(headers_))
      , content_body(std::move(content_))
      , error(error_)
      , stats(std::move(request_stats_))
  {
  }
  http_request_result() {}

  // Non const to allow move semantics
  std::shared_ptr<const http_request_interface> request;
  uint32_t                                      http_response_code;
  std::vector<std::string>                      headers;
  std::vector<uint8_t>                          content_body;
  std::error_code                               error;
  http_request_stats                            stats;

  std::string get_body_as_string() const { return { content_body.begin(), content_body.end() }; }
};
}

#endif
