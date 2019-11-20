/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_HTTP_CLIENT_H
#define ASIO_HTTP_HTTP_CLIENT_H

#include "asio_http/http_client_settings.h"
#include "asio_http/internal/request_data.h"
#include "asio_http/internal/request_manager.h"
#include <asio_http/http_request.h>

#include <boost/asio.hpp>
#include <memory>
#include <thread>

namespace asio_http
{
class http_client
{
public:
  http_client(const http_client_settings& settings, boost::asio::io_context& io_context);

  ~http_client();

  template<typename CompletionToken>
  auto execute_request(CompletionToken&& completion_token, http_request request, std::string cancellation_token)
  {
    boost::asio::async_completion<CompletionToken, void(http_request_result)> init{ completion_token };

    internal::request_data new_request(
      std::make_shared<http_request>(std::move(request)),
      init.completion_handler,
      boost::asio::get_associated_executor(init.completion_handler, boost::asio::system_executor()),
      std::move(cancellation_token));

    m_request_manager->execute_request_async(new_request);

    return init.result.get();
  }

  template<typename CompletionToken>
  auto get(CompletionToken&& completion_token,
           std::string       url_string,
           std::string       cancellation_token = {},
           ssl_settings      ssl                = {})
  {
    http_request request{ http_method::GET,
                          url(std::move(url_string)),
                          http_request::DEFAULT_TIMEOUT_MSEC,
                          std::move(ssl),
                          {},
                          std::vector<std::uint8_t>(),
                          compression_policy::never };

    return execute_request(std::forward<CompletionToken>(completion_token), request, std::move(cancellation_token));
  }

  template<typename CompletionToken>
  auto post(CompletionToken&&         completion_token,
            std::string               url_string,
            std::vector<std::uint8_t> data,
            std::string               content_type,
            std::string               cancellation_token = {},
            ssl_settings              ssl                = {})
  {
    http_request request{
      http_method::POST,        url(std::move(url_string)),           http_request::DEFAULT_TIMEOUT_MSEC,
      std::move(ssl),           { { "Content-Type", content_type } }, std::move(data),
      compression_policy::never
    };

    return execute_request(std::forward<CompletionToken>(completion_token), request, std::move(cancellation_token));
  }

  void cancel_requests(std::string cancellation_token);

private:
  // Declaration (initialization) order is relevant for the members below
  boost::asio::io_context&                   m_io_context;
  std::shared_ptr<internal::request_manager> m_request_manager;
};
}  // namespace asio_http

#endif
