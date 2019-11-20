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
const std::size_t GET_RESOURCE_HEADER_SIZE_EXPECTED = 73;
const std::string TIMEOUT_RESOURCE                  = "/timeout";
const std::string ECHO_RESOURCE                     = "/echo";
const std::string POST_RESOURCE                     = "/post";
const std::string CONNECTION_CLOSE_RESOURCE         = "/close";
const std::string REDIRECTION_RESOURCE              = "/redirect";
const std::string COMPRESSED_RESOURCE               = "/compressed";

const std::string HTTP_CANCELLATION_TOKEN = "asio_httpTest";

const std::vector<uint32_t> COMPRESSED_TEXT = { 0x1f, 0x8b, 0x08, 0x00, 0x64, 0x71, 0xd5, 0x5d, 0x00, 0x03,
                                                0x0b, 0x49, 0x2d, 0x2e, 0xc9, 0xcc, 0x4b, 0x57, 0x48, 0xce,
                                                0xcf, 0x2d, 0x28, 0x4a, 0x2d, 0x2e, 0xce, 0xcc, 0xcf, 0xe3,
                                                0x02, 0x00, 0x4b, 0x67, 0x20, 0xb6, 0x14, 0x00, 0x00, 0x00 };

const std::string UNCOMPRESSED_TEXT = "Testing compression\n";

const std::uint32_t HTTP_CLIENT_POOL_SIZE = 25;

std::string get_url(const std::string& resource)
{
  return HOST + resource;
}

const std::function<void(std::shared_ptr<test_server::web_client>)> get_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Content-type: text/plain\r\n\r\n");
    client_data->response_printf(GET_RESPONSE.c_str());
  };

const std::function<void(std::shared_ptr<test_server::web_client>)> redirection_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Location: http://127.0.0.1:10124/anything\r\n");
    client_data->response_printf("Content-type: text/plain\r\n\r\n");
    client_data->response_printf("Moved");
  };

const std::function<void(std::shared_ptr<test_server::web_client>)> connection_close_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Content-type: text/plain\r\n\r\n");
    client_data->response_printf(GET_RESPONSE.c_str());
    client_data->m_close_connection = true;
  };

const std::function<void(std::shared_ptr<test_server::web_client>)> timeout_handler =
  [](std::shared_ptr<test_server::web_client>) {};

const std::function<void(std::shared_ptr<test_server::web_client>)> echo_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Content-type: text/plain\r\n\r\n");
    const auto data = client_data->get_post_data();
    client_data->response_printf(std::string(data.begin(), data.end()).c_str());
  };

const std::function<void(std::shared_ptr<test_server::web_client>)> compressed_handler =
  [](std::shared_ptr<test_server::web_client> client_data) {
    client_data->response_printf("Content-type: text/plain\r\nContent-Encoding: gzip\r\n\r\n");
    client_data->m_response_buffer.insert(
      std::end(client_data->m_response_buffer), COMPRESSED_TEXT.begin(), COMPRESSED_TEXT.end());
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
      : m_http_client(new http_client(http_client_settings{ HTTP_CLIENT_POOL_SIZE, 5 }, m_test_io_context))
      , m_client_thread([&]() {
        auto work = boost::asio::make_work_guard(m_test_io_context.get_executor());
        m_test_io_context.run();
      })
      , m_web_server(m_test_io_context,
                     "127.0.0.1",
                     10123,
                     { { GET_RESOURCE, get_handler },
                       { CONNECTION_CLOSE_RESOURCE, connection_close_handler },
                       { TIMEOUT_RESOURCE, timeout_handler },
                       { ECHO_RESOURCE, echo_handler },
                       { REDIRECTION_RESOURCE, redirection_handler },
                       { COMPRESSED_RESOURCE, compressed_handler },
                       { POST_RESOURCE,
                         [&](std::shared_ptr<test_server::web_client> client_data) {
                           m_post_data_queue.add_request_post_data(client_data);
                         } } })
      , m_web_server_redirected(m_test_io_context, "127.0.0.1", 10124, { { GET_RESOURCE, get_handler } })

  {
  }

  virtual ~http_test_base()
  {
    m_test_io_context.stop();
    m_client_thread.join();
  }

  // iocontext used to run the client
  boost::asio::io_context      m_test_io_context;
  boost::asio::io_context      m_io_context;
  std::unique_ptr<http_client> m_http_client;

  std::thread m_client_thread;

  post_data_queue         m_post_data_queue;
  test_server::web_server m_web_server;
  test_server::web_server m_web_server_redirected;
};
}  // namespace test
}  // namespace asio_http

#endif  // ASIO_HTTP_HTTP_TEST_BASE_H
