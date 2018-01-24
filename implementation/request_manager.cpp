/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/request_manager.h"

#include "asio_http/http_request_interface.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/completion_handler_invoker.h"
#include "asio_http/internal/curl_easy.h"
#include "asio_http/internal/error_categories.h"
#include "asio_http/internal/logging_functions.h"

#include "loguru.hpp"

#include <boost/asio.hpp>
#include <cinttypes>
#include <curl/curl.h>
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
    , m_curl_multi(m_strand, [this](auto&& handle, auto&& curl_code) { on_curl_request_completed(handle, curl_code); })
{
}

request_manager::~request_manager()
{
  for (const request_data& request : m_requests.get<index_curl>())
  {
    release_curl_easy_handle(request, 0);
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
    auto&                                                 index = m_requests.get<index_cancellation>();
    RequestList::index_iterator<index_cancellation>::type it;
    while ((it = index.find(cancellation_token)) != index.end())
    {
      handle_completed_request(index, it, CURLE_ABORTED_BY_CALLBACK);
    }
  }
}

template<typename Iterator, typename Index>
void request_manager::handle_completed_request(Index& index, const Iterator& iterator, uint32_t curl_code)
{
  create_request_result(*iterator, curl_code);
  index.erase(iterator);
  execute_waiting_requests();
}

void request_manager::release_curl_easy_handle(const request_data& request, uint32_t curl_code)
{
  if (request.m_request_state == request_state::in_progress)
  {
    m_curl_multi.remove_handle(request.m_curl_easy->get_handle());
    m_curl_handle_pool.release_handle(request.m_curl_easy, curl_code != 0);
  }
}

void request_manager::create_request_result(const request_data& request, uint32_t curl_code)
{
  const curl_easy* ceasy = request.m_curl_easy.get();

  http_request_result result(request.m_http_request,
                             ceasy ? ceasy->get_response_code() : 0,
                             ceasy ? ceasy->get_reply_header() : std::vector<std::string>(),
                             ceasy ? ceasy->get_data() : std::vector<uint8_t>(),
                             curl_error::make_error_code(curl_code),
                             get_request_stats(request.m_curl_easy.get(), request.m_creation_time));

  http_request_stats_logging(result);

  completion_handler_invoker::invoke_handler(request, std::move(result));

  release_curl_easy_handle(request, curl_code);
}

void request_manager::on_curl_request_completed(CURL* handle, uint32_t curl_code)
{
  auto& index = m_requests.get<index_curl>();
  auto  it    = index.find(handle);
  if (it != m_requests.get<index_curl>().end())
  {
    handle_completed_request(index, it, curl_code);
  }
}

void request_manager::execute_waiting_requests()
{
  auto& index     = m_requests.get<index_state>();
  auto& indexcurl = m_requests.get<index_curl>();

  const auto active_requests = index.count(request_state::in_progress);
  auto       it              = index.begin();
  if (active_requests < m_settings.max_parallel_requests && it != index.end() &&
      it->m_request_state == request_state::waiting)
  {
    const auto host_port = std::make_pair(it->m_http_request->get_url().host, it->m_http_request->get_url().port);
    const auto handle    = m_curl_handle_pool.get_curl_handle(host_port);
    index.modify(it, [&handle](request_data& request) {
      request.m_curl_easy     = handle;
      request.m_request_state = request_state::in_progress;
    });
    // Iterator 'it' might have been invalidated
    const auto it2 = indexcurl.find(handle->get_handle());
    if (!handle->prepare_curl_easy_handle(*(it2->m_http_request)) || !m_curl_multi.add_handle(handle->get_handle()))
    {
      handle_completed_request(indexcurl, it2, CURLE_BAD_FUNCTION_ARGUMENT);
      LOG_F(ERROR, "Curl error starting web request");
    }
  }
}
}
}
