/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_CURL_HANDLE_POOL_H
#define ASIO_HTTP_CURL_HANDLE_POOL_H

#include <map>
#include <memory>
#include <stack>

namespace asio_http
{
namespace internal
{
class curl_easy;

class curl_handle_pool
{
public:
  std::shared_ptr<curl_easy> get_curl_handle(const std::pair<std::string, uint16_t>& host);
  void                       release_handle(std::shared_ptr<curl_easy> handle, bool clean_up);

private:
  std::map<std::pair<std::string, uint16_t>, std::stack<std::shared_ptr<curl_easy>>> m_curl_handle_pool;
};
}
}

#endif
