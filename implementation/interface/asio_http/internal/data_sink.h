/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_DATA_SINK_H
#define ASIO_HTTP_DATA_SINK_H

#include <sstream>
#include <vector>

namespace asio_http
{
namespace internal
{
class data_sink
{
public:
  data_sink()
      : m_compression(compression::none)
  {
  }
  data_sink(const data_sink&) = delete;
  data_sink(data_sink&&)      = default;

  std::uint32_t             write_callback(const void* data, std::uint32_t size, std::uint32_t count);
  std::vector<std::uint8_t> get_data() const;

  // used to find Content-Encoding: deflate headers
  void header_callback(const std::vector<std::pair<std::string, std::string>>&);

private:
  std::ostringstream m_data;

  enum class compression
  {
    none,
    deflate,
    gzip
  };
  compression m_compression;
};
}  // namespace internal
}  // namespace asio_http

#endif
