/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/http_request.h"

namespace asio_http
{
http_request::http_request(http_method                                      http_method,
                           url                                              url,
                           std::uint32_t                                    timeout_msec,
                           ssl_settings                                     certificates,
                           std::vector<std::pair<std::string, std::string>> http_headers,
                           std::vector<std::uint8_t>                        post_data,
                           compression_policy                               compression_policy)
    : m_http_method(http_method)
    , m_url(url)
    , m_timeout_msec(timeout_msec)
    , m_certificates(certificates)
    , m_http_headers(std::move(http_headers))
    , m_post_data(std::move(post_data))
    , m_compression_policy(compression_policy)
{
}
}  // namespace asio_http
