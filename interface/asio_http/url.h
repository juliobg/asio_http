/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_URL_H
#define ASIO_HTTP_URL_H

#include <cstdint>
#include <string>

namespace asio_http
{
class url
{
public:
  // Construct a url from a string, which must have the following format:
  // [protocol://]host[:port][/path][?query]
  explicit url(const std::string& url_string);

  url(const std::tuple<std::string, std::string, std::uint16_t, std::string, std::string>& tuple);

  std::string to_string() const;

  const std::string   protocol;
  const std::string   host;
  const std::string   path;
  const std::string   query;
  const std::uint16_t port;
};

bool operator==(const url& url1, const url& url2);

bool operator!=(const url& url1, const url& url2);
}  // namespace asio_http

#endif
