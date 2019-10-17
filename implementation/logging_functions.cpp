/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/logging_functions.h"

#include "asio_http/http_request.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/http_client_connection.h"

#define LOGURU_IMPLEMENTATION 1
#include "loguru.hpp"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <string>

namespace asio_http
{
namespace internal
{
namespace
{
#ifdef _DEBUG
std::string hex_str(const std::string& data)
{
  std::stringstream ss;
  ss << std::hex;
  for (auto&& value : data)
  {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(value);
  }
  return ss.str();
}

// try to determine whether the data is printable according to first 50 characters
bool is_printable(const std::string& data)
{
  const std::string::size_type max_size = 50;
  const auto                   end      = (data.size() < max_size ? data.end() : data.begin() + max_size);
  return std::all_of(data.begin(), end, [](auto c) { return std::isprint(c) || std::isspace(c); });
}

void log_data(const std::string& url, const char* msg, size_t size, const std::string& data)
{
  DLOG_F(INFO, "(%s) %s (%zu)", url.c_str(), msg, size);
  DLOG_F(INFO, "%s", is_printable(data) ? data.c_str() : hex_str(data).c_str());
}
#endif
}  // namespace

http_request_stats get_request_stats(std::chrono::steady_clock::time_point creation_time)
{
  http_request_stats stats_ret = {};

  stats_ret.total_time_s = std::chrono::steady_clock::now() - creation_time;

  return stats_ret;
}

void http_request_stats_logging(const http_request_result& result, const std::string& name)
{
  DLOG_F(INFO, "Request to %s completed, result: %s", name.c_str(), result.error.message().c_str());
  DLOG_F(INFO, "  Downloaded bytes: %" PRId64, result.stats.downloaded_bytes);
  DLOG_F(INFO, "  Uploaded bytes: %" PRId64, result.stats.uploaded_bytes);
  DLOG_F(INFO, "  Name lookup time: %.5f s", result.stats.name_lookup_time_s.count());
  DLOG_F(INFO, "  Request execution time: %.5f s", result.stats.total_time_s.count());
  DLOG_F(INFO, "  Download speed: %" PRId64, result.stats.avg_download_speed_bps);
  DLOG_F(INFO, "  Upload speed: %" PRId64, result.stats.avg_upload_speed_bps);
}
}  // namespace internal
}  // namespace asio_http
