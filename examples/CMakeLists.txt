#
#    asio_http: http client library for boost asio
#    Copyright (c) 2017 Julio Becerra Gomez
#    See COPYING for license information.
#

project(asio_http.example)

set(IMPLEMENTATION_SOURCES
   main.cpp
)
set(IMPLEMENTATION_HEADERS
)

find_package(Boost COMPONENTS thread system REQUIRED)

add_executable(${PROJECT_NAME} ${IMPLEMENTATION_SOURCES})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    asio_http
    ${Boost_LIBRARIES}
)

