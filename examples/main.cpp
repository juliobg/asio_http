
#include "asio_http/future_handler.h"
#include "asio_http/http_client.h"
#include "asio_http/http_request_result.h"

#include <boost/asio.hpp>
#include <iostream>
#include <vector>

int main(int argc, char* argv[])
{
  boost::asio::io_context context;
  asio_http::http_client  client({}, context);

  {
    // GET request
    client.get([](asio_http::http_request_result result) { std::cout << result.get_body_as_string(); },
               "www.google.com");
    context.run();
  }

  {
    context.restart();
    // GET request using a future
    auto future = client.get(asio_http::use_std_future, "www.google.com");
    context.run();
    auto body = future.get().get_body_as_string();
    std::cout << body;
  }

  {
    context.restart();
    const std::string          str       = "some data to post";
    const std::vector<uint8_t> post_data = { str.begin(), str.end() };
    client.post([](asio_http::http_request_result result) { std::cout << result.get_body_as_string(); },
                "http://httpbin.org/post",
                post_data,
                "text/plain");
    context.run();
  }

  {
    context.restart();
    // GET request using a future
    auto future = client.get(asio_http::use_std_future, "https://duckduckgo.com");
    context.run();
    auto body = future.get().get_body_as_string();
    std::cout << body;
  }
}
