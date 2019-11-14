#
#    asio_http: http client library for boost asio
#    Copyright (c) 2017-2019 Julio Becerra Gomez
#    See COPYING for license information.
#

cmake_minimum_required(VERSION 3.4)
project(asio_http.test)

set(IMPLEMENTATION_SOURCES
  coro_test.cpp
  http_test.cpp
  io_context_test.cpp
  url_test.cpp
  tuple_ptr_test.cpp
)

if(NOT HAS_CORO)
    list(REMOVE_ITEM IMPLEMENTATION_SOURCES coro_test.cpp)
endif()

set(IMPLEMENTATION_HEADERS
  http_test_base.h
)

add_executable(${PROJECT_NAME} ${IMPLEMENTATION_SOURCES} ${IMPLEMENTATION_HEADERS})
add_test(${PROJECT_NAME} ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    asio_http
    asio_http.TestServer
    gtest
    gtest_main
)


