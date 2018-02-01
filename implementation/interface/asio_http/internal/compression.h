/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_COMPRESSION_H
#define ASIO_HTTP_COMPRESSION_H

#include <cstdint>
#include <vector>

namespace asio_http
{
namespace internal
{
std::vector<uint8_t> compress(const std::vector<uint8_t>& data);

std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int compression_level);

std::vector<uint8_t> decompress_gzip(const std::vector<uint8_t>& data);

std::vector<uint8_t> decompress_deflate(const std::vector<uint8_t>& data);
}
}

#endif
