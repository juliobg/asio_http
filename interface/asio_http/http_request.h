/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_HTTP_REQUEST_H
#define ASIO_HTTP_HTTP_REQUEST_H

#include "asio_http/url.h"

#include <vector>

namespace asio_http
{
// Path to the file holding the private key, the client
// certificate and CA bundle, respectively
struct ssl_settings
{
  ssl_settings(std::string client_private_key_file_,
               std::string client_certificate_file_,
               std::string certificate_authority_bundle_file_)
      : client_private_key_file(client_private_key_file_)
      , client_certificate_file(client_certificate_file_)
      , certificate_authority_bundle_file(certificate_authority_bundle_file_)
  {
  }
  ssl_settings() {}

  std::string client_private_key_file;
  std::string client_certificate_file;
  std::string certificate_authority_bundle_file;
};

enum class http_method
{
  GET,
  POST,
  PUT,
  HEAD
};

enum class compression_policy
{
  never,        // never compress
  when_better,  // compress if data gets smaller after compression
  always        // always compress, even when not smaller
};

class http_request
{
public:
  inline static constexpr std::uint32_t DEFAULT_TIMEOUT_MSEC = 120 * 1000;

  http_request(http_method                                      http_method,
               url                                              url,
               std::uint32_t                                    timeout_msec,
               ssl_settings                                     certificates,
               std::vector<std::pair<std::string, std::string>> http_headers,
               std::vector<std::uint8_t>                        post_data,
               compression_policy                               compression_policy);

  http_method                                      get_http_method() const { return m_http_method; }
  url                                              get_url() const { return m_url; }
  uint32_t                                         get_timeout_msec() const { return m_timeout_msec; }
  std::vector<std::pair<std::string, std::string>> get_http_headers() const { return m_http_headers; }
  std::vector<uint8_t>                             get_post_data() const { return m_post_data; }
  compression_policy get_compress_post_data_policy() const { return m_compression_policy; }
  ssl_settings       get_ssl_settings() const { return m_certificates; }

  http_method                                      m_http_method;
  url                                              m_url;
  std::uint32_t                                    m_timeout_msec;
  ssl_settings                                     m_certificates;
  std::vector<std::pair<std::string, std::string>> m_http_headers;
  std::vector<std::uint8_t>                        m_post_data;
  compression_policy                               m_compression_policy;
};
}  // namespace asio_http

#endif
