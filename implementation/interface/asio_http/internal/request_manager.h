/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_REQUEST_MANAGER_H
#define ASIO_HTTP_REQUEST_MANAGER_H

#include "asio_http/http_client_settings.h"
#include "asio_http/internal/connection_pool.h"
#include "asio_http/internal/http_content.h"
#include "asio_http/internal/request_data.h"

#include <boost/asio.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <system_error>

namespace asio_http
{
class http_request_result;

namespace internal
{
class request_manager : public std::enable_shared_from_this<request_manager>
{
public:
  request_manager(const http_client_settings& settings, boost::asio::io_context& io_context);
  ~request_manager();

  void execute_request_async(request_data request) { async<&request_manager::execute_request>(request); }
  void cancel_requests_async(std::string cancellation_token)
  {
    async<&request_manager::cancel_requests>(cancellation_token);
  }
  void
  on_request_completed_async(http_result_data&& http_result_data, http_stack&& handle, boost::system::error_code ec)
  {
    async<&request_manager::on_request_completed>(std::move(http_result_data), std::move(handle), std::move(ec));
  }

private:
  // This is a work-around as we don't have C++20 lambdas perfect capture in C++17
  template<auto F, typename... Args>
  void async(Args&&... args)
  {
    boost::asio::post(
      m_strand, [ptr = shared_from_this(), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply([ptr = ptr.get()](auto&&... args) { (ptr->*F)(std::move(args)...); }, std::move(args));
      });
  }
  struct index_connection
  {
  };
  struct index_state
  {
  };
  struct index_cancellation
  {
  };

  using request_list = boost::multi_index::multi_index_container<
    request_data,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<index_state>,
        boost::multi_index::composite_key<
          request_data,
          boost::multi_index::member<request_data, request_state, &request_data::m_request_state>,
          boost::multi_index::
            member<request_data, std::chrono::steady_clock::time_point, &request_data::m_creation_time>>>,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<index_connection>,
        boost::multi_index::member<request_data, http_stack, &request_data::m_connection>>,
      boost::multi_index::ordered_non_unique<
        boost::multi_index ::tag<index_cancellation>,
        boost::multi_index::member<request_data, std::string, &request_data::m_cancellation_token>>>>;

  void execute_request(request_data&& request);
  void cancel_requests(const std::string& cancellation_token);
  void execute_waiting_requests();
  void on_request_completed(http_result_data&& http_result_data, http_stack&& handle, boost::system::error_code ec);
  template<typename Iterator, typename Index>
  void handle_completed_request(Index& index, const Iterator& iterator, http_request_result&& result);
  template<typename Iterator, typename Index>
  void cancel_request(Index& index, const Iterator& it);

  const http_client_settings                                  m_settings;
  boost::asio::strand<boost::asio::io_context::executor_type> m_strand;
  connection_pool                                             m_connection_pool;
  request_list                                                m_requests;
};
}  // namespace internal
}  // namespace asio_http

#endif
