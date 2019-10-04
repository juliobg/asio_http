/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_HTTP_REQUEST_INTERFACE_H
#define ASIO_HTTP_HTTP_REQUEST_INTERFACE_H

#include "url.h"

#include <string>
#include <vector>

namespace asio_http
{
class http_request_result;

// Path to the file holding the private key, the client
// certificate and CA bundle, respectively
struct ssl_settings
{
  ssl_settings(std::string client_private_key_file_,
               std::string client_certificate_file_,
               std::string certificate_authority_bundle_file_)
      : client_private_key_file(client_private_key_file_)
      , client_certificate_file(client_certificate_file_)
      , certificate_authority_bundle_file(certificate_authority_bundle_file_)
  {
  }
  ssl_settings() {}

  std::string client_private_key_file;
  std::string client_certificate_file;
  std::string certificate_authority_bundle_file;
};

class http_request_interface
{
public:
  enum class http_method
  {
    GET,
    POST,
    PUT,
    HEAD
  };

  enum class compression_policy
  {
    never,        // never compress
    when_better,  // compress if data gets smaller after compression
    always        // always compress, even when not smaller
  };

public:
  virtual ~http_request_interface() {}

  // Get the type of this request
  virtual http_method get_http_method() const = 0;

  // Get the timeout in milliseconds after which the request is aborted
  virtual std::uint32_t get_timeout_msec() const = 0;

  // Get the url for this request
  virtual url get_url() const = 0;

  // Get the proxy to be used or empty
  virtual std::string get_proxy_address() const = 0;

  // Get SSL settings for this request
  virtual ssl_settings get_ssl_settings() const = 0;

  // Get the http headers to send or an empty list
  virtual std::vector<std::string> get_http_headers() const = 0;

  // Get the data to send in a request or an empty vector
  virtual std::vector<std::uint8_t> get_post_data() const = 0;

  // Should the data returned by get_post_data be compressed with zlib before sending
  virtual compression_policy get_compress_post_data_policy() const = 0;
};
}  // namespace asio_http
#endif
