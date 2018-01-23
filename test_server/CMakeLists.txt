#
#    asio_http: wrapper for integrating libcurl with boost.asio applications
#    Copyright (c) 2017 Julio Becerra Gomez 
#    See COPYING for license information.
#

project(asio_http.TestServer)

#set(INTERFACE_FILES
#  interface/asio_http/TestServer/TestServer.h
#)
#set(IMPLEMENTATION_SOURCES
#)

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
