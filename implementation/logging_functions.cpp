/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/logging_functions.h"

#include "asio_http/http_request.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/curl_easy.h"

#define LOGURU_IMPLEMENTATION 1
#include "loguru.hpp"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <curl/curl.h>
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
}
#ifdef _DEBUG
int curl_debug_logging(CURL* unused, curl_infotype info_type, char* data, size_t size, void* url)
{
  const char* url_string = static_cast<const char*>(url);
  std::string data_string(data, size);  // data is not guaranteed to be 0-terminated
  data_string.erase(data_string.find_last_not_of("\n") + 1);

  switch (info_type)
  {
    case CURLINFO_HEADER_IN:
      LOG_F(INFO, "(%s) Received header (%zu) %s", url, size, data_string.c_str());
      break;
    case CURLINFO_HEADER_OUT:
      LOG_F(INFO, "(%s) Sending header (%zu) %s", url, size, data_string.c_str());
      break;
    case CURLINFO_DATA_IN:
      log_data(url_string, "Received data", size, data_string);
      break;
    case CURLINFO_DATA_OUT:
      log_data(url_string, "Sending data", size, data_string);
      break;
    case CURLINFO_SSL_DATA_IN:
      log_data(url_string, "Received SSL data", size, data_string);
      break;
    case CURLINFO_SSL_DATA_OUT:
      log_data(url_string, "Sending SSL data", size, data_string);
      break;
    case CURLINFO_END:
      break;
    case CURLINFO_TEXT:
      LOG_F(INFO, "(%s) Info: %s", url, data_string.c_str());
      break;
  }

  return 0;
}
#endif

http_request_stats get_request_stats(curl_easy* ceasy, std::chrono::steady_clock::time_point creation_time)
{
  http_request_stats stats_ret = {};

  stats_ret.total_time_s = std::chrono::steady_clock::now() - creation_time;

  if (ceasy != nullptr)
  {
    CURL* handle                     = ceasy->get_handle();
    stats_ret.uploaded_bytes         = ceasy->get_curl_easy_info<curl_off_t>(CURLINFO_CONTENT_LENGTH_UPLOAD_T);
    stats_ret.downloaded_bytes       = ceasy->get_curl_easy_info<curl_off_t>(CURLINFO_CONTENT_LENGTH_DOWNLOAD_T);
    stats_ret.avg_upload_speed_bps   = ceasy->get_curl_easy_info<curl_off_t>(CURLINFO_SPEED_UPLOAD_T);
    stats_ret.avg_download_speed_bps = ceasy->get_curl_easy_info<curl_off_t>(CURLINFO_SPEED_DOWNLOAD_T);
    stats_ret.name_lookup_time_s =
      std::chrono::duration<double>(ceasy->get_curl_easy_info<double>(CURLINFO_NAMELOOKUP_TIME));
  }

  return stats_ret;
}

void http_request_stats_logging(const http_request_result& result)
{
  const std::string& url = result.request->get_url().to_string();
  DLOG_F(INFO, "Request to %s completed, result: %s", url.c_str(), result.error.message().c_str());
  DLOG_F(INFO, "  Downloaded bytes: %" PRId64, result.stats.downloaded_bytes);
  DLOG_F(INFO, "  Uploaded bytes: %" PRId64, result.stats.uploaded_bytes);
  DLOG_F(INFO, "  Name lookup time: %.5f s", result.stats.name_lookup_time_s.count());
  DLOG_F(INFO, "  Request execution time: %.5f s", result.stats.total_time_s.count());
  DLOG_F(INFO, "  Download speed: %" PRId64, result.stats.avg_download_speed_bps);
  DLOG_F(INFO, "  Upload speed: %" PRId64, result.stats.avg_upload_speed_bps);
}
}
}
