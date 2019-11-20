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
std::shared_ptr<http_request> create_redirection(const http_result_data& http_result_data)
{
  const std::string location = get_header(http_result_data.m_headers, "Location");
  const auto&       request  = *http_result_data.m_request;

  if (!location.empty())
  {
    return std::make_shared<http_request>(request.get_http_method(),
                                          url(location),
                                          request.get_timeout_msec(),
                                          request.get_ssl_settings(),
                                          request.get_http_headers(),
                                          request.get_post_data(),
                                          request.get_compress_post_data_policy());
  }
  else
  {
    return {};
  }
}

std::pair<bool, std::shared_ptr<http_request>> process_errors(const boost::system::error_code& ec,
                                                              const http_result_data&          http_result_data)
{
  if (ec)
  {
    if (ec == boost::asio::error::broken_pipe || ec == boost::asio::error::connection_reset ||
        ec == HPE_INVALID_EOF_STATE || ec == boost::asio::error::eof)
    {
      return { true, {} };
    }
  }

  switch (http_result_data.m_status_code)
  {
    case 301:
    case 302:
    case 303:
    case 304:
    case 305:
    case 306:
    case 307:
    case 308:
    {
      auto new_request = create_redirection(http_result_data);
      return { static_cast<bool>(new_request), new_request };
    }

    default:
    {
      break;
    }
  }

  return { false, {} };
}

}  // namespace internal
}  // namespace asio_http
