/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/curl_easy.h"

#include "asio_http/http_request_interface.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/data_sink.h"
#include "asio_http/internal/data_source.h"
#include "asio_http/internal/logging_functions.h"
#include "asio_http/internal/string_list.h"

#include "loguru.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <cctype>
#include <cstring>
#include <string>
#include <zlib.h>

namespace asio_http
{
namespace internal
{
namespace
{
// One hour default timeout
const long DNS_CACHE_TIMEOUT_SEC = 3600;
// Value used to flush DNS cache
const long DNS_CACHE_FLUSH = 0;

using asio_http::http_request_interface;

CURLoption http_method_to_curl_option(http_request_interface::http_method http_method)
{
  static const std::map<http_request_interface::http_method, CURLoption> http_method_to_curl_option{
    { http_request_interface::http_method::GET, CURLOPT_HTTPGET },
    { http_request_interface::http_method::POST, CURLOPT_POST },
    { http_request_interface::http_method::PUT, CURLOPT_PUT },
    { http_request_interface::http_method::HEAD, CURLOPT_HTTPGET }
  };

  return http_method_to_curl_option.at(http_method);
}

size_t write_callback(void* data, size_t size, size_t count, void* user_data)
{
  return static_cast<data_sink*>(user_data)->write_callback(data, size, count);
}

size_t read_callback(char* data, size_t size, size_t count, void* user_data)
{
  return static_cast<data_source*>(user_data)->read_callback(data, size, count);
}

int seek_callback(void* user_data, curl_off_t offset, int origin)
{
  std::ios_base::seekdir mapped_origin;
  switch (origin)
  {
    case SEEK_CUR:
      mapped_origin = std::ios_base::cur;
      break;
    case SEEK_END:
      mapped_origin = std::ios_base::end;
    default:
    case SEEK_SET:
      mapped_origin = std::ios_base::beg;
      break;
  }

  return static_cast<data_source*>(user_data)->seek_callback(offset, mapped_origin) ? CURL_SEEKFUNC_OK
                                                                                    : CURL_SEEKFUNC_CANTSEEK;
}
}

size_t curl_easy::header_callback(void* data, size_t size, size_t count, void* user_data)
{
  curl_easy*   curleasy = static_cast<curl_easy*>(user_data);
  const size_t ret      = size * count;
  std::string  header(static_cast<char*>(data), ret);
  // Trim trailing whitespaces
  header.erase(header.find_last_not_of("\n\r\t ") + 1);
  curleasy->m_reply_header.push_back(header);
  curleasy->m_data_sink->header_callback(header);
  return ret;
}

curl_easy::curl_easy(const std::pair<std::string, uint16_t>& host_port_pair, bool dns_cache_flush)
    : m_host_port_pair(host_port_pair)
    , m_curl_easy_handle(curl_easy_init())
    , m_dns_cache_flush(dns_cache_flush)
{
}

curl_easy::~curl_easy()
{
  curl_easy_cleanup(m_curl_easy_handle);
}

int32_t curl_easy::get_response_code() const
{
  return get_curl_easy_info<long>(CURLINFO_RESPONSE_CODE);
}

std::vector<uint8_t> curl_easy::get_data() const
{
  return m_data_sink.get() ? m_data_sink->get_data() : std::vector<uint8_t>();
}

std::size_t curl_easy::get_post_data_size() const
{
  return m_data_source ? m_data_source->get_size() : 0;
}

template<typename TValue>
void curl_easy::set_curl_easy_option(CURLoption option, TValue value)
{
  if (curl_easy_setopt(m_curl_easy_handle, option, value) != CURLE_OK)
  {
    throw std::runtime_error("Error calling curl_easy_setopt");
  }
}

void curl_easy::set_option_when_not_empy(CURLoption option, std::string string)
{
  if (!string.empty())
  {
    set_curl_easy_option(option, m_string_store.insert(std::move(string)).first->c_str());
  }
}

bool curl_easy::prepare_curl_easy_handle(const http_request_interface& request)
{
  reset(request);

  try
  {
    set_debug_options(request);
    set_post_options(request);
    set_ssl_options(request);
    set_request_options(request);

    // Options below are independent of the current request
    set_curl_easy_option(CURLOPT_WRITEFUNCTION, &write_callback);
    set_curl_easy_option(CURLOPT_WRITEDATA, m_data_sink.get());
    set_curl_easy_option(CURLOPT_HTTPHEADER, m_headers->get());
    // Make sure that curl does not send an expect 100-continue
    m_headers->append("Expect:");
    // It seems libcurl does not automagically decode deflate encoded data, do it manually.
    // Set the 'accept-encoding' headers, check for Content-Encoding, disable automatic decoding.
    set_curl_easy_option(CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    set_curl_easy_option(CURLOPT_HTTP_CONTENT_DECODING, static_cast<long>(0));
    set_curl_easy_option(CURLOPT_HEADERFUNCTION, &header_callback);
    set_curl_easy_option(CURLOPT_HEADERDATA, this);
    // Handle 3XX redirect
    set_curl_easy_option(CURLOPT_FOLLOWLOCATION, static_cast<long>(1));
    set_curl_easy_option(CURLOPT_DNS_CACHE_TIMEOUT, m_dns_cache_flush ? DNS_CACHE_FLUSH : DNS_CACHE_TIMEOUT_SEC);
    m_dns_cache_flush = false;
  }
  catch (const std::exception&)
  {
    return false;
  }

  return true;
}

void curl_easy::set_debug_options(const http_request_interface& request)
{
#ifdef _DEBUG
  set_curl_easy_option(CURLOPT_VERBOSE, static_cast<long>(1));
  set_curl_easy_option(CURLOPT_DEBUGFUNCTION, curl_debug_logging);
  set_option_when_not_empy(CURLOPT_DEBUGDATA, request.get_url().to_string());
#endif
}

void curl_easy::set_post_options(const http_request_interface& request)
{
  if (m_data_source->get_size() != 0)
  {
    set_curl_easy_option(CURLOPT_READFUNCTION, reinterpret_cast<void*>(&read_callback));
    set_curl_easy_option(CURLOPT_SEEKFUNCTION, reinterpret_cast<void*>(&seek_callback));
    set_curl_easy_option(CURLOPT_READDATA, m_data_source.get());
    set_curl_easy_option(CURLOPT_SEEKDATA, m_data_source.get());
    set_curl_easy_option(CURLOPT_POSTFIELDSIZE, static_cast<long>(m_data_source->get_size()));
    m_headers->append(("Content-Length: " + boost::lexical_cast<std::string>(m_data_source->get_size())));
    m_headers->append_from_vector(m_data_source->get_encoding_headers());
  }
  else
  {
    set_curl_easy_option(CURLOPT_READFUNCTION, nullptr);
    set_curl_easy_option(CURLOPT_SEEKFUNCTION, nullptr);
    set_curl_easy_option(CURLOPT_READDATA, nullptr);
    set_curl_easy_option(CURLOPT_SEEKDATA, nullptr);
    set_curl_easy_option(CURLOPT_POSTFIELDSIZE, static_cast<long>(0));
  }
}

void curl_easy::set_ssl_options(const http_request_interface& request)
{
  if (request.get_url().protocol == "https")
  {
    auto ssl = request.get_ssl_settings();
    set_option_when_not_empy(CURLOPT_SSLKEY, ssl.client_private_key_file);
    set_option_when_not_empy(CURLOPT_CAINFO, ssl.certificate_authority_bundle_file);
    set_option_when_not_empy(CURLOPT_SSLCERT, ssl.client_certificate_file);
    // Verify the authenticity of the peer's certificate
    set_curl_easy_option(CURLOPT_SSL_VERIFYPEER, static_cast<long>(1));
    // Verify server certificate
    set_curl_easy_option(CURLOPT_SSL_VERIFYHOST, static_cast<long>(2));
    set_curl_easy_option(CURLOPT_FRESH_CONNECT, static_cast<long>(0));
    set_curl_easy_option(CURLOPT_FORBID_REUSE, static_cast<long>(0));
  }
}

void curl_easy::set_request_options(const http_request_interface& request)
{
  m_headers->append_from_vector(request.get_http_headers());
  set_option_when_not_empy(CURLOPT_PROXY, request.get_proxy_address());
  set_option_when_not_empy(CURLOPT_URL, request.get_url().to_string());
  set_curl_easy_option(http_method_to_curl_option(request.get_http_method()), 1);
  if (request.get_http_method() == http_request_interface::http_method::HEAD)
  {
    set_curl_easy_option(CURLOPT_NOBODY, static_cast<long>(1));
  }
  // Set timeout, do not trigger a SIGALRM on timeout
  set_curl_easy_option(CURLOPT_TIMEOUT_MS, static_cast<long>(request.get_timeout_msec()));
  set_curl_easy_option(CURLOPT_NOSIGNAL, static_cast<long>(1));
}

void curl_easy::reset(const http_request_interface& request)
{
  m_data_sink.reset(new data_sink());
  m_data_source.reset(new data_source(request.get_post_data(), request.get_compress_post_data_policy()));
  m_reply_header.clear();
  curl_easy_reset(m_curl_easy_handle);
  m_headers.reset(new string_list());
  m_string_store.clear();
}
}
}
