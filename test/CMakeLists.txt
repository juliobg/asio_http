#
#    asio_http: wrapper for integrating libcurl with boost.asio applications
#    Copyright (c) 2017 Julio Becerra Gomez 
#    See COPYING for license information.
#

project(asio_http.test)

set(IMPLEMENTATION_SOURCES
  coro_test.cpp
  http_test.cpp
  http_request_test.cpp
  io_context_test.cpp
  url_test.cpp
)
set(IMPLEMENTATION_HEADERS
  http_test_base.h
)

add_executable(${PROJECT_NAME} ${IMPLEMENTATION_SOURCES})

find_package(Boost COMPONENTS filesystem system REQUIRED)
find_package(Curl REQUIRED)

enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
add_executable(${PROJECT_NAME} ${IMPLEMENTATION_SOURCES} ${IMPLEMENTATION_HEADERS})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    asio_http
    asio_http.TestServer
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${GTEST_BOTH_LIBRARIES}
)


