/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/http_error_handling.h"

#include "asio_http/error.h"
#include "asio_http/internal/request_data.h"
#include "asio_http/internal/request_manager.h"

#include <cctype>  // tolower
#include <vector>

namespace asio_http
{
namespace internal
{
namespace
{
bool iequals(const std::string& a, const std::string& b)
{
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) { return tolower(a) == tolower(b); });
}

std::string get_header(const std::vector<std::pair<std::string, std::string>>& headers, const std::string& header)
{
  const auto h =
    std::find_if(std::begin(headers), std::end(headers), [&header](const auto& h) { return iequals(h.first, header); });

  return h != std::end(headers) ? h->second : std::string{};
}

std::shared_ptr<http_request> create_redirection(const request_buffers& request_buffers)
{
  const std::string location = get_header(request_buffers.m_headers, "Location");
  const auto&       request  = *request_buffers.m_request;

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
}  // namespace

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
  }

  switch (request_buffers.m_status_code)
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
      auto new_request = create_redirection(request_buffers);
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
