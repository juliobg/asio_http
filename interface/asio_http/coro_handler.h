/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CORO_HANDLER
#define ASIO_HTTP_CORO_HANDLER

#include "asio_http/http_request_result.h"

#include <boost/asio.hpp>
#include <experimental/coroutine>
#include <memory>

namespace asio_http
{
struct use_coro_t
{
};
constexpr use_coro_t use_coro;
}  // namespace asio_http

namespace boost
{
namespace asio
{
template<typename ReturnType>
class async_result<asio_http::use_coro_t, ReturnType(asio_http::http_request_result)>
{
  struct shared_info
  {
    shared_info()
        : m_has_handle(false)
        , m_has_result(false)
    {
    }
    std::experimental::coroutine_handle<> m_coro;
    bool                                  m_has_handle;
    asio_http::http_request_result        m_result;
    bool                                  m_has_result;
  };

public:
  struct [[nodiscard]] Awaiter
  {
    Awaiter(std::shared_ptr<shared_info> info)
        : m_shared_info(info)
    {
    }
    std::shared_ptr<shared_info>   m_shared_info;
    bool                           await_ready() { return m_shared_info->m_has_result; }
    asio_http::http_request_result await_resume() { return m_shared_info->m_result; }
    void                           await_suspend(std::experimental::coroutine_handle<> coro)
    {
      m_shared_info->m_coro       = coro;
      m_shared_info->m_has_handle = true;
    }
  };

  using return_type = Awaiter;
  std::shared_ptr<shared_info> m_shared_info;

  struct completion_handler_type
  {
    std::shared_ptr<shared_info> m_shared_info;
    template<typename Whatever>
    completion_handler_type(Whatever const&)
        : m_shared_info(std::make_shared<shared_info>())
    {
    }
    void operator()(asio_http::http_request_result result)
    {
      m_shared_info->m_result = std::move(result);
      if (m_shared_info->m_has_handle)
      {
        m_shared_info->m_coro.resume();
      }
    }
  };

  explicit async_result(completion_handler_type& completion_handler)
      : m_shared_info(completion_handler.m_shared_info)
  {
  }

  async_result(const async_result&) = delete;
  async_result& operator=(const async_result&) = delete;
  return_type   get() { return Awaiter(m_shared_info); }
};
}  // namespace asio
}  // namespace boost
#endif
