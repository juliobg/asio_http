
#include "asio_http/future_handler.h"
#include "asio_http/http_client.h"
#include "asio_http/http_request_result.h"

#include <boost/asio.hpp>
#include <iostream>

int main(int argc, char* argv[])
{
  // GET request
  boost::asio::io_context context;
  asio_http::http_client  client({}, context);
  client.get([](asio_http::http_request_result result) { std::cout << result.get_body_as_string(); }, "www.google.com");
  context.run();

  context.restart();
  // GET request using a future
  auto future = client.get(asio_http::use_std_future, "www.google.com");
  context.run();
  auto body = future.get().get_body_as_string();
  std::cout << body;
}
