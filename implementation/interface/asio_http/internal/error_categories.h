/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_ERROR_CATEGORIES_H
#define ASIO_HTTP_ERROR_CATEGORIES_H

#include <curl/curl.h>
#include <system_error>

namespace asio_http
{
namespace internal
{
struct curl_error_category : std::error_category
{
  const char* name() const noexcept override { return "Curl Error"; }
  std::string message(int ev) const override { return curl_easy_strerror(CURLcode(ev)); }
};

class curl_error
{
public:
  static std::error_code make_error_code(int ev) { return { ev, get_singleton() }; }

private:
  static const curl_error_category& get_singleton()
  {
    static const curl_error_category singleton;
    return singleton;
  }
};
}
}

#endif
