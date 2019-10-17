/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/http_error_handling.h"

#include "asio_http/error.h"
#include "asio_http/internal/request_data.h"
#include "asio_http/internal/request_manager.h"

#include <vector>

namespace asio_http
{
namespace internal
{
std::pair<bool, std::shared_ptr<http_request>> process_errors(const boost::system::error_code& ec,
                                                              const request_buffers&           request_buffers)
{
  if (ec)
  {
    if (ec == boost::asio::error::broken_pipe || ec == boost::asio::error::connection_reset ||
        ec == HPE_INVALID_EOF_STATE || ec == boost::asio::error::eof)
    {
      return { true, {} };
    }
    return { false, {} };
  }
  else
  {
    switch (request_buffers.m_status_code)
    {
      default:
        return { false, {} };
    }
  }
}

}  // namespace internal
}  // namespace asio_http
