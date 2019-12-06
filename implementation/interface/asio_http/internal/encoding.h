/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_ENCODING_H
#define ASIO_HTTP_ENCODING_H

#include "asio_http/http_request.h"
#include "asio_http/url.h"
#include "asio_http/internal/tuple_ptr.h"

#include <string>
#include <utility>
#include <vector>
#include <boost/system/error_code.hpp>

namespace asio_http
{
namespace internal
{
enum class compression
{
  none,
  deflate,
  gzip
};

template<std::size_t N, typename Ls>
class encoding : public shared_tuple_base<encoding<N, Ls>>
{
public:
  typename Ls::template type<N - 1>* upper_layer;
  typename Ls::template type<N + 1>* lower_layer;

  void write_headers(http_method method, url url, std::vector<std::pair<std::string, std::string>> headers)
  {
    lower_layer->write_headers(method, url, std::move(headers));
  }

  void on_error(const boost::system::error_code& ec) { upper_layer->on_error(ec); }

  void on_headers(unsigned int status_code, std::vector<std::pair<std::string, std::string>> headers)
  {
    upper_layer->on_headers(status_code, std::move(headers));
  }

  void on_body(const char* at, size_t length) { upper_layer->on_body(at, length); }

  void message_complete() { upper_layer->message_complete(); }

  void close () {lower_layer->close();}

  auto get_body_data(char* at, std::size_t length) { return upper_layer->get_body_data(at, length); }

private:
};
}
}

#endif  // ASIO_HTTP_ENCODING_H
