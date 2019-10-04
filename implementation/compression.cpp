/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/compression.h"

#include <loguru.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <vector>
#include <zlib.h>

namespace
{
const int GZIP_ENCODING = 16;
}

namespace asio_http
{
namespace internal
{
using std::uint8_t;
std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int compression_level)
{
  z_stream             z_str{};
  std::vector<uint8_t> result;

  // We need a gzip header hence calling deflateInit2
  if (deflateInit2(&z_str, compression_level, Z_DEFLATED, (GZIP_ENCODING + MAX_WBITS), 8, Z_DEFAULT_STRATEGY) == Z_OK)
  {
    z_str.next_in  = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(data.data()));
    z_str.avail_in = data.size();

    int                ret;
    std::vector<Bytef> outbuffer(32768);

    // blockwise compression
    do
    {
      z_str.next_out  = outbuffer.data();
      z_str.avail_out = outbuffer.size();

      ret = deflate(&z_str, Z_FINISH);

      if (result.size() < z_str.total_out)
      {
        uint8_t* const compressed_buffer = reinterpret_cast<uint8_t*>(outbuffer.data());
        result.insert(result.end(), compressed_buffer, compressed_buffer + z_str.total_out - result.size());
      }
    } while (ret == Z_OK);

    deflateEnd(&z_str);

    if (ret != Z_STREAM_END)
    {
      result.clear();
      LOG_F(ERROR, "compress: error while attempting gzip compression: %d", ret);
    }
  }
  else
  {
    LOG_F(ERROR, "compress: failed to initialize zlib for gzip compression");
  }

  return result;
}

std::vector<uint8_t> compress(const std::vector<uint8_t>& data)
{
  return compress(data, Z_BEST_COMPRESSION);
}

std::vector<uint8_t> decompress_gzip(const std::vector<uint8_t>& data)
{
  z_stream             z_str{};
  std::vector<uint8_t> result;

  if (inflateInit2(&z_str, (GZIP_ENCODING + MAX_WBITS)) != Z_OK)
  {
    DLOG_F(ERROR, "decompress_gzip: failed to initialize zlib for gzip decompression");
  }
  else
  {
    z_str.next_in  = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(data.data()));
    z_str.avail_in = data.size();

    int                ret;
    std::vector<Bytef> outbuffer(32768);

    do
    {
      z_str.next_out  = outbuffer.data();
      z_str.avail_out = outbuffer.size();

      ret = inflate(&z_str, 0);

      if (result.size() < z_str.total_out)
      {
        uint8_t* inflated_buffer = reinterpret_cast<uint8_t*>(outbuffer.data());
        result.insert(result.end(), inflated_buffer, inflated_buffer + z_str.total_out - result.size());
      }
    } while (ret == Z_OK);

    inflateEnd(&z_str);

    if (ret != Z_STREAM_END)
    {
      LOG_F(ERROR,
            "decompress_gzip: error while attempting gzip "
            "decompression: %d",
            ret);
    }
  }

  return result;
}

std::vector<uint8_t> decompress_deflate(const std::vector<uint8_t>& data)
{
  std::vector<uint8_t> result;
  if (!data.empty())
  {
    // iterate untill the buffer is large enough to hold uncompressed data
    uLongf             size = data.size() * 2;
    std::vector<Bytef> buffer;
    int                error_code;
    do
    {
      size *= 2;
      buffer.resize(size);
      error_code = uncompress(buffer.data(), &size, reinterpret_cast<const Bytef*>(data.data()), data.size());
    } while (error_code == Z_BUF_ERROR);

    if (error_code == Z_OK)
    {
      result.assign(buffer.begin(), buffer.begin() + size);
    }
    else
    {
      LOG_F(ERROR, "decompress_deflate: error while attempting deflate: %d", error_code);
    }
  }
  return result;
}
}  // namespace internal
}  // namespace asio_http
