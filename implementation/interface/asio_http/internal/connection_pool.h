/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CONNECTION_POOL_H
#define ASIO_HTTP_CONNECTION_POOL_H

#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <stack>

namespace asio_http
{
namespace internal
{
class http_client_connection;

class connection_pool
{
public:
  connection_pool(boost::asio::io_context::strand& strand)
      : m_strand(strand)
  {
  }
  std::shared_ptr<http_client_connection> get_connection(const std::pair<std::string, uint16_t>& host);
  void release_connection(std::shared_ptr<http_client_connection> handle, bool clean_up);

private:
  boost::asio::io_context::strand                                                                 m_strand;
  std::map<std::pair<std::string, uint16_t>, std::stack<std::shared_ptr<http_client_connection>>> m_connection_pool;
};
}
}
#endif
