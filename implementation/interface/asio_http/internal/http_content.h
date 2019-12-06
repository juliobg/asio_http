#ifndef ASIO_HTTP_HTTP_CONTENT_H
#define ASIO_HTTP_HTTP_CONTENT_H

#include "asio_http/error.h"
#include "asio_http/http_request.h"
#include "asio_http/http_request_result.h"
#include "asio_http/internal/connection_pool.h"
#include "asio_http/internal/data_sink.h"
#include "asio_http/internal/data_source.h"
#include "asio_http/internal/http_client_connection.h"
#include "asio_http/internal/http_stack_shared.h"
#include "asio_http/internal/socket.h"
#include "asio_http/internal/tuple_ptr.h"

#include "http_parser.h"

#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace asio_http
{
namespace internal
{
struct http_result_data
{
  std::shared_ptr<const http_request>              m_request;
  std::vector<std::pair<std::string, std::string>> m_headers;
  unsigned int                                     m_status_code;
  std::vector<uint8_t>                             data;
};

struct http_stack_interface
{
  virtual void start_async(std::shared_ptr<const http_request>                                request,
                           std::function<void(http_result_data&&, boost::system::error_code)> callback) = 0;

  virtual void cancel_async() = 0;

  virtual std::pair<std::string, std::uint16_t> get_host_and_port() const = 0;

  virtual ~http_stack_interface() {}
};

template<std::size_t N, typename Ls>
class http_content
    : public http_stack_interface
    , public shared_tuple_base<http_content<N, Ls>>
{
public:
  std::shared_ptr<const http_request>                                m_request;
  std::function<void(http_result_data&&, boost::system::error_code)> m_completed_request_callback;
  std::shared_ptr<http_stack_shared>                                 m_shared_data;
  boost::asio::deadline_timer                                        m_timer;
  http_result_data                                                   m_result;

  typename Ls::template type<N + 1>* lower_layer;

  std::unique_ptr<data_sink>   m_body_sink;
  std::unique_ptr<data_source> m_body_source;

  boost::asio::strand<boost::asio::io_context::executor_type>& m_strand;

  http_content(std::shared_ptr<http_stack_shared> shared_data, boost::asio::io_context& context, std::pair<std::string, std::uint16_t> host)
      : m_shared_data(shared_data)
      , m_timer(context)
      , m_strand(shared_data->strand)
      , m_host(host)
  {
  }

  std::pair<std::string, std::uint16_t> get_host_and_port() const override { return m_host; }

  void start(std::shared_ptr<const http_request>                                request,
             std::function<void(http_result_data&&, boost::system::error_code)> callback)
  {
    m_request = request;
    m_body_sink.reset();
    m_body_source.reset(new data_source(request->get_post_data(), request->get_compress_post_data_policy()));
    m_body_sink.reset(new data_sink());

    m_completed_request_callback = std::move(callback);

    m_timer.expires_from_now(boost::posix_time::millisec(request->get_timeout_msec()));
    m_timer.async_wait([ptr = this->shared_from_this()](auto&& ec) {
      if (!ec)
      {
        ptr->complete_request(boost::asio::error::timed_out);
      }
    });

    auto headers = request->get_http_headers();
    headers.emplace_back("Host", m_request->get_url().host);
    if (m_body_source->get_size() != 0)
    {
      headers.emplace_back("Content-Length", std::to_string(m_body_source->get_size()));
    }

    lower_layer->write_headers(request->get_http_method(), request->get_url(), std::move(headers));
  }

  void complete_request(const boost::system::error_code& ec)
  {
    if (m_completed_request_callback != nullptr)
    {
      m_timer.cancel();
      m_result.data      = m_body_sink->get_data();
      m_result.m_request = m_request;
      m_completed_request_callback(std::move(m_result), ec);
      m_result = {};  // After cancellation or timeout it may happen request still running

      m_completed_request_callback = nullptr;
    }
  }

  void start_async(std::shared_ptr<const http_request>                                request,
                   std::function<void(http_result_data&&, boost::system::error_code)> callback) override
  {
    async<&http_content::start>(std::move(request), std::move(callback));
  }

  auto get_body_data(char* at, std::size_t length) { return m_body_source->read_callback(at, length); }

  void on_error(const boost::system::error_code& ec) { complete_request(ec); }

  void on_headers(unsigned int status_code, std::vector<std::pair<std::string, std::string>> headers)
  {
    m_result.m_status_code = status_code;
    m_result.m_headers     = std::move(headers);

    m_body_sink->header_callback(m_result.m_headers);
  }

  void on_body(const char* at, size_t length) { m_body_sink->write_callback(at, length, 1); }

  void message_complete() { complete_request({}); }

  void cancel()
  {
    complete_request(make_error_code(boost::asio::error::operation_aborted));
    lower_layer->close();
  }
  void cancel_async() override { async<&http_content::cancel>(); }

private:
  std::pair<std::string, std::uint16_t> m_host;
  // This is a work-around as we don't have C++20 lambdas perfect capture in C++17
  template<auto F, typename... Args>
  void async(Args&&... args)
  {
    boost::asio::post(
      m_strand, [ptr = this->shared_from_this(), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply([ptr = ptr.get()](auto&&... args) { (ptr->*F)(std::move(args)...); }, std::move(args));
      });
  }
};

}  // namespace internal
}  // namespace asio_http

#endif
