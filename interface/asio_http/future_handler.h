/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_FUTURE_HANDLER
#define ASIO_HTTP_FUTURE_HANDLER

#include "asio_http/http_request_result.h"

#include <boost/asio.hpp>
#include <future>
#include <memory>

namespace asio_http
{
struct use_std_future_t
{
};
constexpr use_std_future_t use_std_future;
}  // namespace asio_http

namespace boost
{
namespace asio
{
template<typename ReturnType>
class async_result<asio_http::use_std_future_t, ReturnType(asio_http::http_request_result)>
{
  std::future<asio_http::http_request_result> m_future;

public:
  using return_type = std::future<asio_http::http_request_result>;

  struct completion_handler_type
  {
    std::shared_ptr<std::promise<asio_http::http_request_result>> m_promise;
    template<typename Whatever>
    completion_handler_type(Whatever const&)
        : m_promise(std::make_shared<std::promise<asio_http::http_request_result>>())
    {
    }
    void operator()(asio_http::http_request_result result) { m_promise->set_value(std::move(result)); }
  };

  explicit async_result(completion_handler_type& completion_handler)
      : m_future(completion_handler.m_promise->get_future())
  {
  }

  async_result(const async_result&) = delete;
  async_result& operator=(const async_result&) = delete;
  return_type   get() { return std::move(m_future); }
};
}  // namespace asio
}  // namespace boost
#endif
