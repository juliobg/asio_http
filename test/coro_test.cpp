/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "http_test_base.h"

#include "asio_http/coro_handler.h"
#include "asio_http/future_handler.h"
#include "asio_http/http_request.h"
#include "asio_http/http_request_interface.h"

#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

template<typename... Args>
struct std::experimental::coroutine_traits<std::future<void>, Args...>
{
  struct promise_type
  {
    std::promise<void>               p;
    auto                             get_return_object() { return p.get_future(); }
    std::experimental::suspend_never initial_suspend() { return {}; }
    std::experimental::suspend_never final_suspend() { return {}; }
    void                             set_exception(std::exception_ptr e) { p.set_exception(std::move(e)); }
    void                             unhandled_exception() { p.set_exception(std::current_exception()); }
    void                             return_void() { p.set_value(); }
  };
};

template<typename R, typename... Args>
struct std::experimental::coroutine_traits<std::future<R>, Args...>
{
  struct promise_type
  {
    std::promise<R>                  p;
    auto                             get_return_object() { return p.get_future(); }
    std::experimental::suspend_never initial_suspend() { return {}; }
    std::experimental::suspend_never final_suspend() { return {}; }
    void                             set_exception(std::exception_ptr e) { p.set_exception(std::move(e)); }
    void                             unhandled_exception() { p.set_exception(std::current_exception()); }
    template<typename U>
    void return_value(U&& u)
    {
      p.set_value(std::forward<U>(u));
    }
  };
};

using namespace asio_http::test_server;

namespace asio_http
{
namespace test
{
namespace
{
std::future<std::vector<http_request_result>> get_three_results(http_client& client)
{
  http_request_result r1 = co_await client.get(use_coro, get_url(GET_RESOURCE));
  http_request_result r2 = co_await client.get(use_coro, get_url(GET_RESOURCE));
  http_request_result r3 = co_await client.get(use_coro, get_url(GET_RESOURCE));

  co_return std::vector<http_request_result>{ r1, r2, r3 };
}
}

class coro_test : public http_test_base
{
public:
  coro_test() { m_http_client.reset(new http_client({}, m_io_context)); }
};

TEST_F(coro_test, get_request)
{
  auto future = get_three_results(*m_http_client);
  m_io_context.run();

  auto results = future.get();

  EXPECT_EQ(3, results.size());

  for (auto& result : results)
  {
    EXPECT_EQ(200, result.http_response_code);
  }
}
}
}
