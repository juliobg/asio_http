/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/data_sink.h"

#include "asio_http/internal/compression.h"

#include "loguru.hpp"

#include <boost/algorithm/string/predicate.hpp>
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

void data_sink::header_callback(const std::string& header)
{
  // Detect whether compression is used and which one
  if (boost::icontains(header, "Content-Encoding:"))
  {
    if (header.find("deflate") != std::string::npos)
    {
      m_compression = compression::deflate;
    }
    else if (header.find("gzip") != std::string::npos)
    {
      m_compression = compression::gzip;
    }
    else
    {
      LOG_F(ERROR, "Unknown content encoding");
    }
  }
}
}  // namespace internal
}  // namespace asio_http
