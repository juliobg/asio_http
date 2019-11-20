/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
    */

#ifndef ASIO_HTTP_ERROR_HANDLING_H
#define ASIO_HTTP_ERROR_HANDLING_H

#include "asio_http/http_request.h"
#include "asio_http/internal/http_content.h"

#include <boost/system/error_code.hpp>
#include <memory>
#include <utility>

namespace asio_http
{
namespace internal
{
std::pair<bool, std::shared_ptr<http_request>> process_errors(const boost::system::error_code& ec,
                                                              const http_result_data&          http_result_data);

}  // namespace internal
}  // namespace asio_http
#endif
