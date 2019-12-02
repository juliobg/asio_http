/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/data_sink.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/compression.h"

#include "loguru.hpp"

#include <vector>

namespace asio_http
{
namespace internal
{
using std::uint32_t;
using std::uint8_t;

uint32_t data_sink::write_callback(const void* data, uint32_t size, uint32_t count)
{
  uint32_t ret = size * count;
  m_data.write(static_cast<const char*>(data), ret);
  return ret;
}

std::vector<uint8_t> data_sink::get_data() const
{
  const std::string& str = m_data.str();
  switch (m_compression)
  {
    case compression::none:
      return { str.begin(), str.end() };
    case compression::deflate:
      return decompress_deflate(std::vector<uint8_t>(str.begin(), str.end()));
    case compression::gzip:
      return decompress_gzip(std::vector<uint8_t>(str.begin(), str.end()));
  }
}

void data_sink::header_callback(const std::vector<std::pair<std::string, std::string>>& headers)
{
  const auto value = get_header(headers, "Content-Encoding");

  if (iequals(value, "deflate"))
  {
    m_compression = compression::deflate;
  }
  else if (iequals(value, "gzip"))
  {
    m_compression = compression::gzip;
  }
  else if (!value.empty())
  {
    LOG_F(ERROR, "Unknown content encoding");
  }
}
}  // namespace internal
}  // namespace asio_http
