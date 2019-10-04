#
#    asio_http: http client library for boost asio
#    Copyright (c) 2017-2019 Julio Becerra Gomez
#    See COPYING for license information.
#

project(http_parser)

set(INTERFACE_FILES
  http_parser.h
)

set(IMPLEMENTATION_SOURCES
  http_parser.c
)

set(IMPLEMENTATION_HEADERS
)

add_library(${PROJECT_NAME} STATIC
  ${INTERFACE_FILES}
  ${IMPLEMENTATION_SOURCES}
  ${IMPLEMENTATION_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    .
)
