/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/request_manager.h"

#include "asio_http/error.h"
#include "asio_http/http_request_interface.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/completion_handler_invoker.h"
#include "asio_http/internal/http_client_connection.h"
#include "asio_http/internal/logging_functions.h"

#include "loguru.hpp"

#include <boost/asio.hpp>
#include <cinttypes>
#include <memory>
#include <utility>
#include <vector>

namespace asio_http
{
namespace internal
{
request_manager::request_manager(const http_client_settings& settings, boost::asio::io_context& io_context)
    : m_settings(settings)
    , m_strand(io_context)
    , m_connection_pool(m_strand)
{
}

request_manager::~request_manager()
{
  auto& index = m_requests.get<index_connection>();
  for (auto it = index.begin(); it != index.end(); ++it)
  {
    if (m_requests.get<index_connection>().begin()->m_connection)
    {
      m_requests.get<index_connection>().begin()->m_connection->cancel(true);
    }
  }
}

void request_manager::execute_request(const request_data& request)
{
  m_requests.insert(request);
  execute_waiting_requests();
  DLOG_F(INFO, "New request added");
}

void request_manager::cancel_requests(const std::string& cancellation_token)
{
  if (!cancellation_token.empty())
  {
    auto& index = m_requests.get<index_cancellation>();

    request_list::index_iterator<index_cancellation>::type it;
    while ((it = index.find(cancellation_token)) != index.end())
    {
      cancel_request(index, it);
    }
  }
}

template<typename Iterator, typename Index>
void request_manager::cancel_request(Index& index, const Iterator& it)
{
  if (it->m_connection)
  {
    it->m_connection->cancel();
  }
  else
  {
    handle_completed_request(index, it, boost::asio::error::operation_aborted);
  }
}

template<typename Iterator, typename Index>
void request_manager::handle_completed_request(Index& index, const Iterator& iterator, std::error_code ec)
{
  create_request_result(*iterator, ec);
  index.erase(iterator);
  m_strand.post([ptr = this->shared_from_this()]() { ptr->execute_waiting_requests(); });
}

void request_manager::release_http_client_connection_handle(const request_data& request, std::error_code ec)
{
  if (request.m_request_state == request_state::in_progress)
  {
    m_connection_pool.release_connection(request.m_connection, static_cast<bool>(ec));
  }
}

void request_manager::create_request_result(const request_data& request, std::error_code ec)
{
  const http_client_connection* connection = request.m_connection.get();

  http_request_result result(request.m_http_request,
                             connection ? connection->get_response_code() : 0,
                             connection ? connection->get_reply_headers() : std::vector<std::string>(),
                             connection ? connection->get_data() : std::vector<std::uint8_t>(),
                             ec,
                             get_request_stats(request.m_connection.get(), request.m_creation_time));

  http_request_stats_logging(result);

  completion_handler_invoker::invoke_handler(request, std::move(result));

  release_http_client_connection_handle(request, ec);
}

void request_manager::on_request_completed(std::shared_ptr<http_client_connection> handle, boost::system::error_code ec)
{
  auto& index = m_requests.get<index_connection>();
  auto  it    = index.find(handle);
  if (it != m_requests.get<index_connection>().end())
  {
    if ((ec == boost::asio::error::broken_pipe || ec == boost::asio::error::connection_reset ||
         ec == HPE_INVALID_EOF_STATE) &&
        it->m_retries == 0)
    {
      m_connection_pool.release_connection(it->m_connection, static_cast<bool>(ec));
      const auto new_handle = m_connection_pool.get_connection(handle->get_host_and_port());
      const auto request    = it->m_http_request;
      index.modify(it, [&new_handle](request_data& request) {
        request.m_connection = new_handle;
        request.m_retries++;
      });
      new_handle->start(request, [this](auto&& handle, auto&& ec) { this->on_request_completed(handle, ec); });
    }
    else
    {
      handle_completed_request(index, it, asio_mapped_error::convert(ec));
    }
  }
}

void request_manager::execute_waiting_requests()
{
  auto& index = m_requests.get<index_state>();

  const auto active_requests = index.count(request_state::in_progress);
  auto       it              = index.begin();
  if (active_requests < m_settings.max_parallel_requests && it != index.end() &&
      it->m_request_state == request_state::waiting)
  {
    const auto host_port = std::make_pair(it->m_http_request->get_url().host, it->m_http_request->get_url().port);
    const auto handle    = m_connection_pool.get_connection(host_port);
    const auto request   = it->m_http_request;
    index.modify(it, [&handle](request_data& request) {
      request.m_connection    = handle;
      request.m_request_state = request_state::in_progress;
    });
    handle->start(request, [this](auto&& handle, auto&& ec) { this->on_request_completed(handle, ec); });
  }
}
}  // namespace internal
}  // namespace asio_http
