/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_HTTP_TEST_BASE_H
#define ASIO_HTTP_HTTP_TEST_BASE_H

#include "asio_http/future_handler.h"
#include "asio_http/http_client.h"
#include "asio_http/http_request.h"
#include "asio_http/http_request_interface.h"
#include "asio_http/http_request_result.h"
#include "asio_http/test_server/test_server.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>

namespace asio_http
{
namespace test
{
namespace
{
const std::string HOST                              = "http://127.0.0.1:10123";
const std::string GET_RESOURCE                      = "/anything";
const std::string GET_RESPONSE                      = "This is the response";
const std::size_t GET_RESOURCE_HEADER_SIZE_EXPECTED = 81;
const std::string TIMEOUT_RESOURCE                  = "/timeout";
const std::string ECHO_RESOURCE                     = "/echo";
const std::string POST_RESOURCE                     = "/post";

const std::string HTTP_CANCELLATION_TOKEN = "asio_httpTest";

const std::uint32_t HTTP_CLIENT_POOL_SIZE = 25;

std::string get_url(const std::string resource)
{
  return HOST + resource;
}

const std::function<void(std::shared_ptr<test_server::web_client>)> get_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Content-type: text/plain\r\n\r\n");
    client_data->response_printf(GET_RESPONSE.c_str());
  };

class timeout_callable
{
public:
  std::vector<std::shared_ptr<test_server::web_client>> web_clients;

  void operator()(std::shared_ptr<test_server::web_client> client_data) { web_clients.push_back(client_data); }
};

const std::function<void(std::shared_ptr<test_server::web_client>)> echo_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Content-type: text/plain\r\n\r\n");
    const auto data = client_data->get_post_data();
    client_data->response_printf(std::string(data.begin(), data.end()).c_str());
  };
}  // namespace

class post_data_queue
{
public:
  std::future<std::string> get_next_request_post_data()
  {
    std::lock_guard<std::mutex> lock(m_data_mutex);
    std::promise<std::string>   promise;
    std::future<std::string>    future = promise.get_future();
    if (m_requests_without_promise.empty())
    {
      m_promises.push_back(std::move(promise));
    }
    else
    {
      promise.set_value(std::move(m_requests_without_promise.front()));
      m_requests_without_promise.pop_front();
    }
    return future;
  }

  void add_request_post_data(std::shared_ptr<test_server::web_client> client_data)
  {
    const auto                  postData = client_data->get_post_data();
    const std::string           data(postData.begin(), postData.end());
    std::lock_guard<std::mutex> lock(m_data_mutex);
    if (m_promises.empty())
    {
      m_requests_without_promise.push_back(data);
    }
    else
    {
      m_promises.front().set_value(data);
      m_promises.pop_front();
    }
  }

  std::deque<std::string>               m_requests_without_promise;
  std::deque<std::promise<std::string>> m_promises;
  std::mutex                            m_data_mutex;
};

class http_test_base : public ::testing::Test
{
protected:
  http_test_base()
      : m_http_client(new http_client(http_client_settings{ HTTP_CLIENT_POOL_SIZE }, m_client_io_context))
      , m_client_thread([&]() { auto work = boost::asio::make_work_guard(m_client_io_context.get_executor()); m_client_io_context.run(); })
      , m_web_server(m_client_io_context,
                     "127.0.0.1",
                     10123,
                     { { GET_RESOURCE, get_handler },
                       { TIMEOUT_RESOURCE, timeout_callable() },
                       { ECHO_RESOURCE, echo_handler },
                       { POST_RESOURCE,
                         [&](std::shared_ptr<test_server::web_client> client_data) {
                           m_post_data_queue.add_request_post_data(client_data);
                         } } })
  {
  }

  virtual ~http_test_base()
  {
    m_client_io_context.stop();
    m_client_thread.join();
  }

  // iocontext used to run the client
  boost::asio::io_context m_client_io_context;
  boost::asio::io_context      m_io_context;
  std::unique_ptr<http_client> m_http_client;

  std::thread m_client_thread;

  post_data_queue m_post_data_queue;
  test_server::web_server m_web_server;
};
}  // namespace test
}  // namespace asio_http

#endif  // HTTPTESTBASE_H
