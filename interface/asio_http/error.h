/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
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
struct generic_category : public std::error_category
{
  virtual const char* name() const noexcept override { return boost::system::generic_category().name(); }
  virtual std::string message(int value) const override { return boost::system::generic_category().message(value); }
};

struct system_category : public std::error_category
{
  virtual const char* name() const noexcept override { return boost::system::system_category().name(); }
  virtual std::string message(int value) const override { return boost::system::system_category().message(value); }
};

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

class asio_mapped_error
{
public:
  static std::error_code convert(const boost::system::error_code& error)
  {
    if (error.category() == boost::system::generic_category())
    {
      return std::error_code(error.value(), get_singleton_generic());
    }
    else if (error.category() == boost::system::system_category())
    {
      return std::error_code(error.value(), get_singleton_system());
    }
    return std::error_code(error.value(), get_singleton_generic());
  }

  static const generic_category& get_singleton_generic()
  {
    static const generic_category singleton;
    return singleton;
  }

  static const system_category& get_singleton_system()
  {
    static const system_category singleton;
    return singleton;
  }
};
}  // namespace internal
}  // namespace asio_http

namespace std
{
template<>
struct is_error_code_enum<boost::system::errc::errc_t> : public std::true_type
{
};

inline std::error_code make_error_code(boost::system::errc::errc_t e)
{
  return std::error_code(static_cast<int>(e), asio_http::internal::asio_mapped_error::get_singleton_generic());
}

template<>
struct is_error_code_enum<boost::asio::error::basic_errors> : public std::true_type
{
};

inline std::error_code make_error_code(boost::asio::error::basic_errors e)
{
  return std::error_code(static_cast<int>(e), asio_http::internal::asio_mapped_error::get_singleton_system());
}
}  // namespace std

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

inline boost::system::error_code make_error_code(http_errno e)
{
  return boost::system::error_code(static_cast<int>(e), asio_http::internal::http_parser_error::get_singleton());
}

#endif
