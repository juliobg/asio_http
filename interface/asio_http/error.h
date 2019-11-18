/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_ERROR_CATEGORIES_H
#define ASIO_HTTP_ERROR_CATEGORIES_H

#include "http_parser.h"

#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>

#include <system_error>

namespace asio_http
{
namespace internal
{
struct http_parser_category : public boost::system::error_category
{
  virtual const char* name() const noexcept override { return "HTTP parser error"; }
  virtual std::string message(int value) const override { return http_errno_name(http_errno(value)); }
};

class http_parser_error
{
public:
  static boost::system::error_code make_error_code(int ev) { return { ev, get_singleton() }; }

  static const http_parser_category& get_singleton()
  {
    static const http_parser_category singleton;
    return singleton;
  }
};
}  // namespace internal
}  // namespace asio_http

inline boost::system::error_code make_error_code(http_errno e)
{
  return boost::system::error_code(static_cast<int>(e), asio_http::internal::http_parser_error::get_singleton());
}

namespace boost
{
namespace system
{
template<>
struct is_error_code_enum<http_errno>
{
  static const bool value = true;
};
}  // namespace system
}  // namespace boost
#endif
