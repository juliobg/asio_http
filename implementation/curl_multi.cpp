/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/curl_multi.h"

#include <memory>

namespace asio_http
{
namespace internal
{
namespace
{
void socket_action(curl_multi_data& multi_data, curl_socket_t socket, int event_mask);
void update_monitor(std::shared_ptr<curl_socket> curl_socket);

void process_sockets_on_io_event(std::shared_ptr<curl_socket>     curl_socket,
                                 int                              action,
                                 const boost::system::error_code& error)
{
  if (curl_socket->asio_stream.is_open())
  {
    if (error == boost::asio::error::operation_aborted)
    {
      socket_action(*(curl_socket->curl_multi_data), curl_socket->asio_stream.native_handle(), CURL_CSELECT_ERR);
    }
    else
    {
      socket_action(*(curl_socket->curl_multi_data), curl_socket->asio_stream.native_handle(), action);
    }

    curl_socket->pending_read  = curl_socket->pending_read && action != CURL_POLL_IN;
    curl_socket->pending_write = curl_socket->pending_write && action != CURL_POLL_OUT;

    update_monitor(std::move(curl_socket));
  }
}

void update_monitor(std::shared_ptr<curl_socket> curl_socket)
{
  // Check whether the asio socket is still alive because the call to curl_multi_socket_action in
  // ProcessSocketsOnIoEvent may have triggered a call to CloseSocket in the mean time
  if (curl_socket->asio_stream.is_open() && curl_socket->curl_multi_data->strand != nullptr)
  {
    auto strand = *curl_socket->curl_multi_data->strand;
    if (curl_socket->read_monitor && !curl_socket->pending_read)
    {
      curl_socket->asio_stream.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                                          boost::asio::bind_executor(strand, [cs = curl_socket](auto&& error) mutable {
                                            process_sockets_on_io_event(std::move(cs), CURL_POLL_IN, error);
                                          }));
      curl_socket->pending_read = true;
    }
    if (curl_socket->write_monitor && !curl_socket->pending_write)
    {
      curl_socket->asio_stream.async_wait(boost::asio::posix::stream_descriptor::wait_write,
                                          boost::asio::bind_executor(strand, [cs = curl_socket](auto&& error) mutable {
                                            process_sockets_on_io_event(std::move(cs), CURL_POLL_OUT, error);
                                          }));

      curl_socket->pending_write = true;
    }
  }
}

void process_multi_info(curl_multi_data& multi_data)
{
  CURLMsg* msg;
  int      msgs_left;

  while ((msg = curl_multi_info_read(multi_data.curl_multi_handle, &msgs_left)))
  {
    if (msg->msg == CURLMSG_DONE)
    {
      // The callback function must remove the handle from curl multi
      multi_data.completed_request_callback(msg->easy_handle, msg->data.result);
    }
  }
}

void socket_action(curl_multi_data& multi_data, curl_socket_t socket, int event_mask)
{
  if (multi_data.curl_multi_handle != nullptr)
  {
    int number_of_handles;

    curl_multi_socket_action(multi_data.curl_multi_handle, socket, event_mask, &number_of_handles);

    process_multi_info(multi_data);

    if (number_of_handles <= 0)
    {
      multi_data.timer.cancel();
    }
  }
}
}

curl_multi::curl_multi(boost::asio::io_context::strand&                      strand,
                       std::function<void(CURL* handle, uint32_t curl_code)> completed_request_callback)
    : m_curl_multi_data(
        std::make_shared<curl_multi_data>(curl_multi_init(), strand, std::move(completed_request_callback)))
{
  curl_multi_setopt(m_curl_multi_data->curl_multi_handle, CURLMOPT_SOCKETFUNCTION, curl_multi::socket_callback);
  curl_multi_setopt(m_curl_multi_data->curl_multi_handle, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(m_curl_multi_data->curl_multi_handle, CURLMOPT_TIMERFUNCTION, curl_multi::timer_callback);
  curl_multi_setopt(m_curl_multi_data->curl_multi_handle, CURLMOPT_TIMERDATA, this);
}

bool curl_multi::add_handle(CURL* easy_handle)
{
  bool result = false;

  if (easy_handle != nullptr)
  {
    result = (curl_multi_add_handle(m_curl_multi_data->curl_multi_handle, easy_handle) == CURLM_OK);
  }

  return result;
}

void curl_multi::remove_handle(CURL* easy_handle)
{
  if (easy_handle)
  {
    curl_multi_remove_handle(m_curl_multi_data->curl_multi_handle, easy_handle);
  }
}

int curl_multi::socket_callback(CURL* easy, curl_socket_t socket, int action, void* callback_data, void* socket_data)
{
  curl_multi* self = static_cast<curl_multi*>(callback_data);

  self->set_socket(socket, easy, action);

  return 0;
}

void curl_multi::set_socket(curl_socket_t socket, CURL* easy, int action)
{
  // Curl informs us it is not interested in IO events for this socket
  if (action == CURL_POLL_REMOVE)
  {
    unregister_socket(socket);
  }
  else
  {
    const auto it = m_socket_map.find(socket);

    std::shared_ptr<curl_socket> curl_socket_ptr;

    // Socket alredy being monitored
    if (it != m_socket_map.end())
    {
      curl_socket_ptr = it->second;
    }
    // This is a new socket asio is not aware of
    else
    {
      curl_socket_ptr = std::make_shared<curl_socket>(m_curl_multi_data->strand->context(), m_curl_multi_data);
      curl_socket_ptr->asio_stream.assign(socket);
      register_socket(socket, curl_socket_ptr);
    }

    curl_socket_ptr->read_monitor  = (action == CURL_POLL_IN) || (action == CURL_POLL_INOUT);
    curl_socket_ptr->write_monitor = (action == CURL_POLL_OUT) || (action == CURL_POLL_INOUT);

    update_monitor(curl_socket_ptr);
  }
}

int curl_multi::timer_callback(CURLM*, long timeout_in_ms, curl_multi* curl_multi_instance)
{
  if (timeout_in_ms >= 0)
  {
    curl_multi_instance->m_curl_multi_data->timer.expires_from_now(boost::posix_time::millisec(timeout_in_ms));
    curl_multi_instance->m_curl_multi_data->timer.async_wait(boost::asio::bind_executor(
      *(curl_multi_instance->m_curl_multi_data->strand), [ptr = curl_multi_instance->m_curl_multi_data](auto&& ec) {
        socket_action(*ptr, CURL_SOCKET_TIMEOUT, 0);
      }));
  }
  // curl uses value -1 to indicate that the timer must be cancelled
  else if (timeout_in_ms == -1)
  {
    curl_multi_instance->m_curl_multi_data->timer.cancel();
  }

  return 0;
}
}
}
