/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "http_test_base.h"

#include "asio_http/future_handler.h"
#include "asio_http/http_request.h"

#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

using namespace asio_http::test_server;

namespace asio_http
{
namespace test
{
class io_context_test : public http_test_base
{
public:
  io_context_test() { m_http_client.reset(new http_client({}, m_io_context)); }
};

TEST_F(io_context_test, get_request)
{
  auto result_future = m_http_client->get(use_std_future, get_url(GET_RESOURCE), HTTP_CANCELLATION_TOKEN);

  m_io_context.run();

  auto result = result_future.get();

  EXPECT_EQ(200, result.http_response_code);
}

TEST_F(io_context_test, request_manager_destruction)  // To be run with ASAN configuration
{
  for (int i = 0; i < 1000; i++)
  {
    auto result_future = m_http_client->get(use_std_future, get_url(GET_RESOURCE), HTTP_CANCELLATION_TOKEN);

    m_http_client.reset(new http_client({}, m_io_context));

    m_io_context.run();
  }
}

TEST_F(io_context_test, executor_binding)
{
  const std::size_t num_requests = 100;

  // Thread pool executor with only one thread
  boost::asio::thread_pool thread_pool(1);

  // Get thread ID for this executor
  std::packaged_task<std::thread::id()> task([]() { return std::this_thread::get_id(); });
  std::thread::id                       id = boost::asio::post(thread_pool.get_executor(), std::move(task)).get();

  for (std::size_t i = 0; i < num_requests; ++i)
  {
    auto callable = boost::asio::bind_executor(
      thread_pool.get_executor(), [id](const http_request_result&) { EXPECT_EQ(id, std::this_thread::get_id()); });
    m_http_client->get(callable, get_url(GET_RESOURCE), HTTP_CANCELLATION_TOKEN);
  }

  m_io_context.run();
}
}  // namespace test
}  // namespace asio_http
