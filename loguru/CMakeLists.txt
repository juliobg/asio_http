#
#    asio_http: http client library for boost asio
#    Copyright (c) 2017-2019 Julio Becerra Gomez
#    See COPYING for license information.
#

project(loguru)

#set(INTERFACE_FILES
#  loguru.hpp
#)

set(IMPLEMENTATION_SOURCES
)

set(IMPLEMENTATION_HEADERS
)

add_library(${PROJECT_NAME} INTERFACE
)

target_include_directories(${PROJECT_NAME}
  INTERFACE
    .
)

