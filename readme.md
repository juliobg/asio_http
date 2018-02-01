asio_http
=========

This is an http client library for Boost.Asio


Features
--------
* **simple interface** - Download and upload anything, synchronously or asynchronously, with just a few lines.
* **connection pool** - It is possible to configure the maximum number of parallel asynchronous requests. Pending request are enqueued.
* **completion handler flexibility** - Any type of callables along futures or C++20 (experimental) awaitables are supported.

Requirements
------------
* Boost 1.66
* zlib
* C++14

It has been tested with Clang 5.0 and macOS 10.13. It should work with any C++14 compliant compiler, except for the coroutines handler and tests, which are only enabled for Clang 5.

Installation
------------
It is a cmake based project. Just copy the asio_http folder into your (also cmake based) project, and add asio_http as a target link library dependency.

GET request example
-------------------

This example code shows a GET request whose result is handled by a lambda to print the body.

```c++
#include "asio_http/http_client.h"
#include "asio_http/http_request_result.h"

#include <boost/asio.hpp>
#include <iostream>

int main(int argc, char* argv[])
{
  boost::asio::io_context context;

  asio_http::http_client  client({}, context);

  client.get([](asio_http::http_request_result result) { std::cout << result.get_body_as_string(); }, "www.google.com");

  context.run();
}
```
