#
#    asio_http: http client library for boost asio
#    Copyright (c) 2017-2019 Julio Becerra Gomez
#    See COPYING for license information.
#

project(asio_http.TestServer)

add_library(${PROJECT_NAME} INTERFACE
)

find_package(Boost REQUIRED)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${Boost_LIBRARIES}
)

target_include_directories(${PROJECT_NAME}
  INTERFACE
    interface
)
