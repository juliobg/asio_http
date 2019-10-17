/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_HTTP_REQUEST_H
#define ASIO_HTTP_HTTP_REQUEST_H

#include "asio_http/http_request_interface.h"
#include "asio_http/url.h"

#include <memory>
#include <vector>

namespace asio_http
{
class http_request : public http_request_interface
{
public:
  static const std::uint32_t DEFAULT_TIMEOUT_MSEC = 120 * 1000;

  http_request(http_method               http_method,
               url                       url,
               std::uint32_t             timeout_msec,
               ssl_settings              certificates,
               std::vector<std::string>  http_headers,
               std::vector<std::uint8_t> post_data,
               compression_policy        compression_policy);

  virtual http_method              get_http_method() const override { return m_http_method; }
  virtual url                      get_url() const override { return m_url; }
  virtual uint32_t                 get_timeout_msec() const override { return m_timeout_msec; }
  virtual std::vector<std::string> get_http_headers() const override { return m_http_headers; }
  virtual std::vector<uint8_t>     get_post_data() const override { return m_post_data; }
  virtual compression_policy       get_compress_post_data_policy() const override { return m_compression_policy; }
  virtual ssl_settings             get_ssl_settings() const override { return m_certificates; }

  const http_method               m_http_method;
  const url                       m_url;
  const std::uint32_t             m_timeout_msec;
  const ssl_settings              m_certificates;
  const std::vector<std::string>  m_http_headers;
  const std::vector<std::uint8_t> m_post_data;
  const compression_policy        m_compression_policy;
};
}  // namespace asio_http

#endif
