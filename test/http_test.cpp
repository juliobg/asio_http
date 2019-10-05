/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "http_test_base.h"

#include "asio_http/error.h"
#include "asio_http/future_handler.h"
#include "asio_http/http_request.h"
#include "asio_http/http_request_interface.h"

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
              return a1 + a2.size();
            }));
}

TEST_F(http_test, head_request)
{
  auto request = std::make_shared<http_request>(http_request_interface::http_method::HEAD,
                                                url(get_url(GET_RESOURCE)),
                                                std::string(),
                                                120000,
                                                ssl_settings(),
                                                std::vector<std::string>(),
                                                std::vector<uint8_t>(),
                                                http_request_interface::compression_policy::never);

  auto reply = m_http_client->execute_request(use_std_future, request, HTTP_CANCELLATION_TOKEN).get();

  EXPECT_FALSE(reply.error);
  EXPECT_EQ(200, reply.http_response_code);
  EXPECT_EQ(0, reply.content_body.size());
  EXPECT_EQ(GET_RESOURCE_HEADER_SIZE_EXPECTED,
            std::accumulate(reply.headers.begin(), reply.headers.end(), 0, [](const auto& a1, const auto& a2) {
              return a1 + a2.size();
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

TEST_F(http_test, request_destroyed)
{
  auto request = std::make_shared<http_request>(http_request_interface::http_method::GET,
                                                url(get_url(GET_RESOURCE)),
                                                std::string(),
                                                120000,
                                                ssl_settings(),
                                                std::vector<std::string>(),
                                                std::vector<uint8_t>(),
                                                http_request_interface::compression_policy::never);

  std::weak_ptr<http_request_interface> weak(request);
  EXPECT_FALSE(weak.expired());
  m_http_client->execute_request(use_std_future, std::move(request), HTTP_CANCELLATION_TOKEN).get();

  // Execute a new request for synchronization purposes. I.e., before accepting and completing the
  // new request, the previous request must be deleted
  m_http_client->get(use_std_future, get_url(GET_RESOURCE)).get();

  EXPECT_TRUE(weak.expired());
}

TEST_F(http_test, timeout)
{
  // Request with 1 second timeout
  const auto request = std::make_shared<http_request>(http_request_interface::http_method::GET,
                                                      url(get_url(TIMEOUT_RESOURCE)),
                                                      std::string(),
                                                      1000,
                                                      ssl_settings(),
                                                      std::vector<std::string>(),
                                                      std::vector<uint8_t>(),
                                                      http_request_interface::compression_policy::never);

  // Execute request and wait until it is ready
  http_request_result reply = m_http_client->execute_request(use_std_future, request, HTTP_CANCELLATION_TOKEN).get();

  // Check data
  EXPECT_EQ(std::make_error_code(boost::asio::error::timed_out), reply.error);
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
  const std::size_t number_of_requests = 30;

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

TEST_F(http_test, cancel_requests)
{
  std::future<http_request_result> future =
    m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE), HTTP_CANCELLATION_TOKEN);

  m_http_client->cancel_requests(HTTP_CANCELLATION_TOKEN);

  EXPECT_EQ(std::make_error_code(boost::asio::error::operation_aborted), future.get().error);
}

TEST_F(http_test, empty_cancellation_token)
{
  std::future<http_request_result> future = m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE), "");

  m_http_client->cancel_requests("");

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Request should be cancelled
  EXPECT_EQ(std::make_error_code(boost::asio::error::operation_aborted), future.get().error);
}

TEST_F(http_test, shutdown_in_progress)
{
  std::future<http_request_result> reply = m_http_client->get(use_std_future, get_url(TIMEOUT_RESOURCE));

  // sleep for one second so the IO thread can pick up the request
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // shutdown the request manager while the request is in progress
  m_http_client.reset();

  // get should throw an exception, as the promise is destroyed
  EXPECT_EQ(std::make_error_code(boost::asio::error::operation_aborted), reply.get().error);
}
}  // namespace test
}  // namespace asio_http
