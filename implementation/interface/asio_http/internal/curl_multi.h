/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CURL_MULTI_H
#define ASIO_HTTP_CURL_MULTI_H

#include <boost/asio.hpp>
#include <curl/curl.h>
#include <functional>
#include <memory>

namespace asio_http
{
namespace internal
{
struct curl_multi_data
{
  curl_multi_data(CURLM*                                                curl_multi_handle,
                  boost::asio::io_context::strand&                      strand,
                  std::function<void(CURL* handle, uint32_t curl_code)> completed_request_callback)
      : curl_multi_handle(curl_multi_handle)
      , strand(&strand)
      , completed_request_callback(std::move(completed_request_callback))
      , timer(strand.context())
  {
  }
  CURLM*                                                curl_multi_handle;
  boost::asio::io_context::strand*                      strand;
  std::function<void(CURL* handle, uint32_t curl_code)> completed_request_callback;
  boost::asio::deadline_timer                           timer;
};

struct curl_socket
{
  curl_socket(boost::asio::io_context& io_context, std::shared_ptr<curl_multi_data> curl_multi_data)
      : asio_stream(io_context)
      , read_monitor()
      , write_monitor()
      , pending_read()
      , pending_write()
      , curl_multi_data(std::move(curl_multi_data))
  {
  }
  boost::asio::posix::stream_descriptor asio_stream;
  bool                                  read_monitor;
  bool                                  write_monitor;
  bool                                  pending_read;
  bool                                  pending_write;
  std::shared_ptr<curl_multi_data>      curl_multi_data;
};

class curl_multi
{
public:
  curl_multi(boost::asio::io_context::strand&                      strand,
             std::function<void(CURL* handle, uint32_t curl_code)> completed_request_callback);
  curl_multi(const curl_multi& other) = delete;

  ~curl_multi()
  {
    curl_multi_cleanup(m_curl_multi_data->curl_multi_handle);
    m_curl_multi_data->curl_multi_handle = nullptr;
    m_curl_multi_data->strand            = nullptr;
  }

  bool add_handle(CURL* easy_handle);

  // This is essentially equivalent to canceling a request.
  void remove_handle(CURL* easy_handle);

private:
  void register_socket(curl_socket_t curl_native_socket, std::shared_ptr<curl_socket> curl_socket)
  {
    m_socket_map[curl_native_socket] = curl_socket;
  }

  void unregister_socket(curl_socket_t curl_socket)
  {
    const auto it = m_socket_map.find(curl_socket);

    if (it != m_socket_map.end())
    {
      it->second->asio_stream.release();
      m_socket_map.erase(it);
    }
  }

  // We wrap CurlSocket into a shared_ptr in order to solve sync problems with boost asio
  // Even after cleaning up the map some callback may be pending in asio queue
  static int socket_callback(CURL* easy, curl_socket_t socket, int action, void* callback_data, void* socket_data);
  static int timer_callback(CURLM*, long timeout_in_ms, curl_multi* curl_multi);
  void       set_socket(curl_socket_t socket, CURL* easy, int action);

  std::shared_ptr<curl_multi_data>                      m_curl_multi_data;
  std::map<curl_socket_t, std::shared_ptr<curl_socket>> m_socket_map;
};
}
}

#endif  // CURLMULTI_H
