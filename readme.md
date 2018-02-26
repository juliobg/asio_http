[![Build Status](https://travis-ci.org/juliobg/asio_http.svg?branch=master)](https://travis-ci.org/juliobg/asio_http)

asio_http
=========

This is an http client library for Boost.Asio


Features
--------
* **simple interface** - Download and upload anything, synchronously or asynchronously, with just a few lines.
* **connection pool** - It is possible to configure the maximum number of parallel asynchronous requests. Pending request are enqueued.
* **completion handler flexibility** - Any type of callables along futures or C++20 (experimental) awaitables are supported.
* **SSL support**

Requirements
------------
* Boost 1.66
* zlib
* C++14
* OpenSSL

It should work with any C++14 compliant compiler, except for the coroutines handler and tests, which are only enabled for Clang 5.

Installation
------------
It is a cmake based project. Just copy the asio_http folder into your (also cmake based) project, and add asio_http as a target link library dependency.

In any case, if you want to compile and run the tests and examples, just follow the typical cmake procedure:

```
cd asio_http
mkdir build
cd build
cmake .. -DBUILD_ASIO_HTTP_TESTS=ON
make
```

Note we need to set the `BUILD_ASIO_HTTP_TESTS` option in order to build the tests, otherwise only the library is built.

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

Note that HTTPS is supported:

```c++
client.get([](asio_http::http_request_result result) { std::cout << result.get_body_as_string(); }, "https://duckduckgo.com");

```

POST request example
--------------------

Similarly to the previous example, the code below would send a POST HTTP request.

```c++
const std::string          str       = "some data to post";
const std::vector<uint8_t> post_data = { str.begin(), str.end() };
client.post([](asio_http::http_request_result result) { std::cout << result.get_body_as_string(); },
            "http://httpbin.org/post",
            post_data,
            "text/plain");
context.run();
```

Asynchronous
------------
An asynchronous function returns before it is finished, and generally causes some work to happen in the background before triggering some future action in the application (as opposed to normal synchronous functions, which do everything they are going to do before returning).

Currently three asynchronous interfaces have been tested:
* Callback argument
* Future placeholder
* Awaitable placeholder (C++20 coroutines)

For the callback argument see the examples above. Any type of callable is supported (e.g. std::function or C++11 lambda).

In the case of `std::future` placeholder, the special value `asio_http::use_std_future` (arternatively `boost::asio::use_future`) must be used to specify that an asynchronous operation should return a future.

```c++
#include "asio_http/future_handler.h"

auto future = client.get(asio_http::use_std_future, "www.google.com");
auto body = future.get().get_body_as_string();
```

See `coro_test.cpp` for an example on how to use the special value `asio_http::use_coro`, which indicates that the asynchronous operation should return a C++20 awaitable.
