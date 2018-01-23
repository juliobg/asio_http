/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_DATA_SOURCE_H
#define ASIO_HTTP_DATA_SOURCE_H

#include "asio_http/http_request_interface.h"

#include <sstream>
#include <utility>
#include <vector>

namespace asio_http
{
namespace internal
{
class data_source
{
public:
  data_source(const data_source&) = delete;
  data_source(std::vector<uint8_t> data, http_request_interface::compression_policy policy);

  size_t read_callback(char* data, size_t size, size_t count);

  // this is needed if the peer is using a 3XX redirect
  bool seek_callback(int32_t offset, std::ios_base::seekdir origin);

  std::size_t get_size() const { return m_size; }

  std::vector<std::string> get_encoding_headers() { return m_encoding_headers; }

private:
  data_source(const std::pair<std::vector<uint8_t>, std::vector<std::string>>& data)
      : m_data(std::string(data.first.begin(), data.first.end()))
      , m_size(data.first.size())
      , m_encoding_headers(data.second)
  {
  }
  std::istringstream       m_data;
  std::size_t              m_size;
  std::vector<std::string> m_encoding_headers;
};
}
}

#endif
