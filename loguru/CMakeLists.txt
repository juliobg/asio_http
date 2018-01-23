#
#    asio_http: wrapper for integrating libcurl with boost.asio applications
#    Copyright (c) 2017 Julio Becerra Gomez 
#    See COPYING for license information.
#

project(Loguru)

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

