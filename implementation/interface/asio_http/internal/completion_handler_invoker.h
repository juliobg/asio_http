/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_COMPLETION_HANDLER_INVOKER_H
#define ASIO_HTTP_COMPLETION_HANDLER_INVOKER_H

namespace asio_http
{
class http_request_result;
namespace internal
{
struct request_data;

class completion_handler_invoker
{
public:
  static void invoke_handler(const request_data& request_data, http_request_result result);
};
}  // namespace internal
}  // namespace asio_http

#endif
