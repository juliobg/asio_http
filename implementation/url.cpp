/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/url.h"

#include <boost/lexical_cast.hpp>
#include <regex>

namespace asio_http
{
namespace
{
using std::uint16_t;

const uint16_t DEFAULT_PORT_HTTP  = 80;
const uint16_t DEFAULT_PORT_HTTPS = 443;

const char* const PROTOCOL_HTTP  = "http";
const char* const PROTOCOL_HTTPS = "https";

auto get_url_components(const std::string& url)
{
  static const std::regex r("^((.+):\\/\\/)?([A-Za-z0-9\\-\\.]+)(:([0-9]+))?(\\/[^?]*)?(\\?.*)?$");
  std::smatch             what;
  if (!std::regex_match(url, what, r))
  {
    throw std::runtime_error("Failed to parse url: '" + url + "'");
  }

  const std::string protocol = (what[2].matched ? what[2].str() : std::string(PROTOCOL_HTTP));

  uint16_t port = 0;
  if (what[5].length() > 0)
  {
    port = boost::lexical_cast<uint16_t>(what[5].str());
  }
  else if (protocol == PROTOCOL_HTTP)
  {
    port = DEFAULT_PORT_HTTP;
  }
  else if (protocol == PROTOCOL_HTTPS)
  {
    port = DEFAULT_PORT_HTTPS;
  }

  const std::string host  = what[3];
  const std::string path  = what[6].str().empty() ? "/" : what[6].str();
  const std::string query = what[7];

  return std::make_tuple(protocol, host, port, path, query);
}
}  // namespace

url::url(const std::tuple<std::string, std::string, uint16_t, std::string, std::string>& tuple)
    : protocol(std::get<0>(tuple))
    , host(std::get<1>(tuple))
    , port(std::get<2>(tuple))
    , path(std::get<3>(tuple))
    , query(std::get<4>(tuple))
{
}

url::url(const std::string& url_string)
    : url(get_url_components(url_string))
{
}

std::string url::to_string() const
{
  std::stringstream stream;
  stream << protocol << "://" << host;

  if (!(protocol == PROTOCOL_HTTP && port == DEFAULT_PORT_HTTP) &&
      !(protocol == PROTOCOL_HTTPS && port == DEFAULT_PORT_HTTPS) && (port != 0))
  {
    stream << ":" << port;
  }
  stream << path << query;
  return stream.str();
}

bool operator==(const url& url1, const url& url2)
{
  return url1.protocol == url2.protocol && url1.host == url2.host && url1.port == url2.port && url1.path == url2.path &&
    url1.query == url2.query;
}

bool operator!=(const url& url1, const url& url2)
{
  return !(url1 == url2);
}
}  // namespace asio_http
