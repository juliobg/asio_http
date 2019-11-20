#ifndef ASIO_HTTP_HTTP_STACK_SHARED_H
#define ASIO_HTTP_HTTP_STACK_SHARED_H

#include <boost/asio.hpp>

namespace asio_http
{
namespace internal
{
struct http_stack_shared
{
  http_stack_shared(boost::asio::io_context& context)
      : strand(context.get_executor())
  {
  }
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
};
}  // namespace internal
}  // namespace asio_http
#endif  // HTTP_STACK_SHARED_H
