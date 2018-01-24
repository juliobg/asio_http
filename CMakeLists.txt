#
#    asio_http: wrapper for integrating libcurl with boost.asio applications
#    Copyright (c) 2017 Julio Becerra Gomez 
#    See COPYING for license information.
#

project(asio_http)

set(CORO_FLAGS "-fcoroutines-ts -stdlib=libc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ${CORO_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG")

INCLUDE_DIRECTORIES(/opt/local/include)
LINK_DIRECTORIES(/opt/local/lib)
find_package(Boost COMPONENTS thread REQUIRED)
find_package(Curl REQUIRED)
find_package(ZLIB REQUIRED)

set(INTERFACE_FILES
  interface/asio_http/coro_handler.h
  interface/asio_http/future_handler.h
  interface/asio_http/http_request.h
  interface/asio_http/http_request_result.h
  interface/asio_http/http_client.h
  interface/asio_http/http_client_settings.h
  interface/asio_http/http_request_interface.h
  interface/asio_http/url.h
)
set(IMPLEMENTATION_SOURCES
  implementation/completion_handler_invoker.cpp
  implementation/curl_easy.cpp
  implementation/curl_handle_pool.cpp
  implementation/curl_multi.cpp
  implementation/data_sink.cpp
  implementation/data_source.cpp
  implementation/http_client.cpp
  implementation/request_manager.cpp
  implementation/logging_functions.cpp
  implementation/string_list.cpp
  implementation/compression.cpp
  implementation/http_request.cpp
  implementation/url.cpp
)
set(IMPLEMENTATION_HEADERS
  implementation/interface/asio_http/internal/completion_handler_invoker.h
  implementation/interface/asio_http/internal/curl_easy.h
  implementation/interface/asio_http/internal/curl_handle_pool.h
  implementation/interface/asio_http/internal/curl_multi.h
  implementation/interface/asio_http/internal/error_categories.h
  implementation/interface/asio_http/internal/data_sink.h
  implementation/interface/asio_http/internal/data_source.h
  implementation/interface/asio_http/internal/request_manager.h
  implementation/interface/asio_http/internal/logging_functions.h
  implementation/interface/asio_http/internal/request_data.h
  implementation/interface/asio_http/internal/string_list.h
  implementation/interface/asio_http/internal/compression.h
)
add_library(${PROJECT_NAME} STATIC
  ${INTERFACE_FILES}
  ${IMPLEMENTATION_SOURCES}
  ${IMPLEMENTATION_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${Boost_LIBRARIES}
  PRIVATE
    Loguru
    ${ZLIB_LIBRARIES}
    ${CURL_LIBRARIES}
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    implementation/interface
    interface
)

add_subdirectory(test)
add_subdirectory(test_server)
add_subdirectory(loguru)
add_subdirectory(examples)
