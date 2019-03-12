/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_REQUEST_MANAGER_H
#define ASIO_HTTP_REQUEST_MANAGER_H

#include "asio_http/http_client_settings.h"
#include "asio_http/internal/connection_pool.h"
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
class request_data;

class request_manager : public std::enable_shared_from_this<request_manager>
{
public:
  request_manager(const http_client_settings& settings, boost::asio::io_context& io_context);
  ~request_manager();
  boost::asio::io_context::strand& get_strand() { return m_strand; }
  void                             execute_request(const request_data& request);
  void                             cancel_requests(const std::string& cancellation_token);

private:
  struct index_connection
  {
  };
  struct index_request
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
        boost::multi_index::member<request_data, std::shared_ptr<http_client_connection>, &request_data::m_connection>>,
      boost::multi_index::ordered_non_unique<boost::multi_index::tag<index_request>,
                                             boost::multi_index::member<request_data,
                                                                        std::shared_ptr<const http_request_interface>,
                                                                        &request_data::m_http_request>>,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<index_cancellation>,
        boost::multi_index::member<request_data, std::string, &request_data::m_cancellation_token>>>>;

  void execute_waiting_requests();
  void on_request_completed(std::shared_ptr<http_client_connection> handle, boost::system::error_code ec);
  template<typename Iterator, typename Index>
  void handle_completed_request(Index& index, const Iterator& iterator, std::error_code ec);
  template<typename Iterator, typename Index>
  void cancel_request(Index& index, const Iterator& it);
  void create_request_result(const request_data& request, std::error_code ec);
  void release_http_client_connection_handle(const request_data& request, std::error_code ec);

  const http_client_settings      m_settings;
  boost::asio::io_context::strand m_strand;
  connection_pool                 m_connection_pool;
  request_list                    m_requests;
};
}  // namespace internal
}  // namespace asio_http

#endif
