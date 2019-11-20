/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "http_test_base.h"

#include "asio_http/error.h"
#include "asio_http/future_handler.h"
#include "asio_http/http_request.h"

#include <boost/system/error_code.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

using namespace asio_http::test_server;

namespace asio_http
{
namespace test
{
using std::uint8_t;

class http_test : public http_test_base
{
};

TEST_F(http_test, get_request)
{
  http_request_result reply = m_http_client->get(use_std_future, get_url(GET_RESOURCE), HTTP_CANCELLATION_TOKEN).get();

  EXPECT_FALSE(reply.error);
  EXPECT_EQ(200, reply.http_response_code);
  EXPECT_EQ(GET_RESPONSE, reply.get_body_as_string());
  EXPECT_EQ(GET_RESOURCE_HEADER_SIZE_EXPECTED,
            std::accumulate(reply.headers.begin(), reply.headers.end(), 0, [](const auto& a1, const auto& a2) {
              return a1 + a2.first.size() + a2.second.size();
            }));
}

TEST_F(http_test, head_request)
{
  http_request request{ http_method::HEAD, url(get_url(GET_RESOURCE)), 120000, {}, {}, {}, compression_policy::never };

  auto reply = m_http_client->execute_request(use_std_future, request, HTTP_CANCELLATION_TOKEN).get();

  EXPECT_FALSE(reply.error);
  EXPECT_EQ(200, reply.http_response_code);
  EXPECT_EQ(0, reply.content_body.size());
  EXPECT_EQ(GET_RESOURCE_HEADER_SIZE_EXPECTED,
            std::accumulate(reply.headers.begin(), reply.headers.end(), 0, [](const auto& a1, const auto& a2) {
              return a1 + a2.first.size() + a2.second.size();
            }));
}

TEST_F(http_test, post_request)
{
  const std::string postdata = "some post data";

  // Execute request and wait until it is ready
  auto reply =
    m_http_client->post(use_std_future, get_url(ECHO_RESOURCE), { postdata.begin(), postdata.end() }, "text/plain")
      .get();

  // Check data
  EXPECT_FALSE(reply.error);
  EXPECT_EQ(200, reply.http_response_code);
  EXPECT_EQ(postdata, reply.get_body_as_string());
}

TEST_F(http_test, timeout)
{
  // Request with 1 second timeout
  http_request request{ http_method::GET, url(get_url(TIMEOUT_RESOURCE)), 1000, {}, {}, {}, compression_policy::never };

  // Execute request and wait until it is ready
  http_request_result reply = m_http_client->execute_request(use_std_future, request, HTTP_CANCELLATION_TOKEN).get();

  // Check data
  EXPECT_EQ(make_error_code(boost::asio::error::timed_out), reply.error);
}

TEST_F(http_test, handle_pool)
{
  std::vector<std::future<http_request_result>> futures;
  futures.emplace_back(m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE), "anyid"));
  for (std::size_t i = 1; i < HTTP_CLIENT_POOL_SIZE; ++i)
  {
    futures.emplace_back(m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE), HTTP_CANCELLATION_TOKEN));
  }
  std::future<http_request_result> blocked_future(
    m_http_client->get(use_std_future, get_url(GET_RESOURCE), HTTP_CANCELLATION_TOKEN));
  for (const auto& future : futures)
  {
    EXPECT_EQ(future.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
  }

  // If the pool allows more than poolsize requests, give it time to execute
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // The request should still be blocked
  EXPECT_EQ(blocked_future.wait_for(std::chrono::seconds(0)), std::future_status::timeout);

  // Cancel one of the blocking requests
  m_http_client->cancel_requests("anyid");

  // The normal request should start and be finished soon
  blocked_future.wait();

  // The other requests should still be stuck in the timeout
  for (std::size_t i = 1; i < HTTP_CLIENT_POOL_SIZE; ++i)
  {
    EXPECT_EQ(futures[i].wait_for(std::chrono::seconds(0)), std::future_status::timeout);
  }
}

TEST_F(http_test, parallel_get_requests)
{
  const std::size_t number_of_requests = 1000;

  std::vector<std::future<http_request_result>> futures;

  for (std::size_t i = 0; i < number_of_requests; ++i)
  {
    futures.push_back(m_http_client->get(use_std_future, get_url(GET_RESOURCE)));
  }

  for (auto& future : futures)
  {
    http_request_result reply = future.get();
    EXPECT_FALSE(reply.error);
    EXPECT_EQ(200, reply.http_response_code);
    EXPECT_EQ(GET_RESPONSE, reply.get_body_as_string());
  }
}

TEST_F(http_test, parallel_get_requests_connection_close)
{
  // Test will probably fail if connections are not properly closed
  const std::size_t number_of_requests = 1000;

  std::vector<std::future<http_request_result>> futures;

  for (std::size_t i = 0; i < number_of_requests; ++i)
  {
    futures.push_back(m_http_client->get(use_std_future, get_url(CONNECTION_CLOSE_RESOURCE)));
  }

  for (auto& future : futures)
  {
    http_request_result reply = future.get();
    EXPECT_FALSE(reply.error);
    if (reply.error)
      EXPECT_EQ(reply.error.message(), "aaaaa");
    EXPECT_EQ(200, reply.http_response_code);
    EXPECT_EQ(GET_RESPONSE, reply.get_body_as_string());
  }
}

TEST_F(http_test, cancel_requests)
{
  std::future<http_request_result> future =
    m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE), HTTP_CANCELLATION_TOKEN);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  m_http_client->cancel_requests(HTTP_CANCELLATION_TOKEN);

  EXPECT_EQ(make_error_code(boost::asio::error::operation_aborted), future.get().error);
}

TEST_F(http_test, empty_cancellation_token)
{
  std::future<http_request_result> future = m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE), "");

  std::this_thread::sleep_for(std::chrono::seconds(1));

  m_http_client->cancel_requests("");

  // Request should be cancelled
  EXPECT_EQ(make_error_code(boost::asio::error::operation_aborted), future.get().error);
}

TEST_F(http_test, shutdown_in_progress)
{
  std::future<http_request_result> reply = m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE));

  // sleep for one second so the IO thread can pick up the request
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // shutdown the request manager while the request is in progress
  m_http_client.reset();

  // request should be completed with an error
  EXPECT_EQ(make_error_code(boost::asio::error::operation_aborted), reply.get().error);
}

TEST_F(http_test, redirected_request)
{
  http_request_result reply =
    m_http_client->get(use_std_future, get_url(REDIRECTION_RESOURCE), HTTP_CANCELLATION_TOKEN).get();

  EXPECT_FALSE(reply.error);
  EXPECT_EQ(200, reply.http_response_code);
  EXPECT_EQ(GET_RESPONSE, reply.get_body_as_string());
  EXPECT_EQ(GET_RESOURCE_HEADER_SIZE_EXPECTED,
            std::accumulate(reply.headers.begin(), reply.headers.end(), 0, [](const auto& a1, const auto& a2) {
              return a1 + a2.first.size() + a2.second.size();
            }));
}

TEST_F(http_test, compressed_response)
{
  http_request_result reply =
    m_http_client->get(use_std_future, get_url(COMPRESSED_RESOURCE), HTTP_CANCELLATION_TOKEN).get();

  EXPECT_FALSE(reply.error);
  EXPECT_EQ(200, reply.http_response_code);
  EXPECT_EQ(UNCOMPRESSED_TEXT, reply.get_body_as_string());
}

}  // namespace test
}  // namespace asio_http
