/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/http_request.h"

namespace asio_http
{
const uint32_t http_request::DEFAULT_TIMEOUT_MSEC;

http_request::http_request(http_method              http_method,
                           url                      url,
                           std::string              proxy,
                           uint32_t                 timeout_msec,
                           ssl_settings             certificates,
                           std::vector<std::string> http_headers,
                           std::vector<uint8_t>     post_data,
                           compression_policy       compression_policy)
    : m_http_method(http_method)
    , m_url(url)
    , m_proxy(proxy)
    , m_timeout_msec(timeout_msec)
    , m_certificates(certificates)
    , m_http_headers(std::move(http_headers))
    , m_post_data(std::move(post_data))
    , m_compression_policy(compression_policy)
{
}

std::shared_ptr<http_request_interface> http_request_builder::create_request() const
{
  return std::make_shared<http_request>(
    http_method, request_url, proxy, timeout_msec, certificates, http_headers, post_data, compress_data);
}
}
