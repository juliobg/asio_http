/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CURL_EASY_H
#define ASIO_HTTP_CURL_EASY_H

#include <asio_http/http_request_result.h>

#include <curl/curl.h>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace asio_http
{
class http_request_interface;

namespace internal
{
class data_sink;
class data_source;
class string_list;
class curl_easy
{
public:
  curl_easy(const std::pair<std::string, uint16_t>& host_port_pair, bool dns_cache_flush);
  curl_easy(const curl_easy&) = delete;
  ~curl_easy();

  std::vector<uint8_t> get_data() const;

  std::size_t get_post_data_size() const;

  template<typename TValue>
  TValue get_curl_easy_info(CURLINFO info) const
  {
    TValue result{};
    return curl_easy_getinfo(m_curl_easy_handle, info, &result) == CURLE_OK ? result : TValue{};
  }
  int32_t get_response_code() const;

  CURL* get_handle() { return m_curl_easy_handle; }

  std::pair<std::string, uint16_t> get_host_and_port() { return m_host_port_pair; }

  bool prepare_curl_easy_handle(const http_request_interface& web_request);

  std::vector<std::string> get_reply_header() const { return m_reply_header; }

private:
  static size_t header_callback(void* data, size_t size, size_t count, void* user_data);
  template<typename TValue>
  void set_curl_easy_option(CURLoption option, TValue value);
  void set_option_when_not_empy(CURLoption option, std::string string);

  void set_debug_options(const http_request_interface& request);
  void set_post_options(const http_request_interface& request);
  void set_ssl_options(const http_request_interface& request);
  void set_request_options(const http_request_interface& request);
  void reset(const http_request_interface& request);

  const std::pair<std::string, uint16_t> m_host_port_pair;
  CURL*                                  m_curl_easy_handle;

  // data sink for receiving data
  std::unique_ptr<data_sink> m_data_sink;

  // data source for sending data
  std::unique_ptr<data_source> m_data_source;

  // optional manually set headers
  std::unique_ptr<string_list> m_headers;

  // flush DNS cache for next request
  bool m_dns_cache_flush;

  std::vector<std::string> m_reply_header;

  std::set<std::string> m_string_store;
};
}
}

#endif  // CURLEASY_H
