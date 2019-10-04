/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/http_request.h"

#include <gtest/gtest.h>
#include <memory>

namespace asio_http
{
namespace test
{
namespace
{
const std::string              URL          = "http://someaddress.com/";
const uint32_t                 TIMEOUT_MSEC = 30 * 1000;
const std::string              CLIENT_PRIVATE_KEY_FILE("ClientPrivateKeyFile");
const std::string              CERTIFICATE_AUTHORITY_BUNDLE_FILE("CertificateAuthorityBundleFile");
const std::string              CLIENT_CERTIFICATE_FILE("ClientCertificateFile");
const std::string              PROXY("proxy");
const std::vector<std::string> HTTP_HEADERS{ "a", "b", "c" };
const std::vector<uint8_t>     POST_DATA{ 8, 2, 0 };
}  // namespace

TEST(http_request_test, get_with_default_values)
{
  std::shared_ptr<http_request_interface> request =
    http_request_builder(URL, http_request_interface::http_method::GET).create_request();

  EXPECT_EQ(http_request_interface::http_method::GET, request->get_http_method());
  EXPECT_EQ(URL, request->get_url().to_string());
  EXPECT_EQ("", request->get_proxy_address());
  EXPECT_EQ(http_request::DEFAULT_TIMEOUT_MSEC, request->get_timeout_msec());
  EXPECT_EQ(0, request->get_http_headers().size());
  EXPECT_EQ(0, request->get_post_data().size());
  EXPECT_EQ("", request->get_ssl_settings().certificate_authority_bundle_file);
  EXPECT_EQ("", request->get_ssl_settings().client_private_key_file);
  EXPECT_EQ("", request->get_ssl_settings().client_certificate_file);
}

TEST(http_request_test, get_with_custom_values)
{
  http_request_builder builder(URL, http_request_interface::http_method::GET);
  builder.timeout_msec                                   = TIMEOUT_MSEC;
  builder.proxy                                          = PROXY;
  builder.certificates.certificate_authority_bundle_file = CERTIFICATE_AUTHORITY_BUNDLE_FILE;
  builder.certificates.client_certificate_file           = CLIENT_CERTIFICATE_FILE;
  builder.certificates.client_private_key_file           = CLIENT_PRIVATE_KEY_FILE;
  builder.http_headers                                   = HTTP_HEADERS;
  builder.post_data                                      = POST_DATA;
  auto request                                           = builder.create_request();

  EXPECT_EQ(http_request_interface::http_method::GET, request->get_http_method());
  EXPECT_EQ(URL, request->get_url().to_string());
  EXPECT_EQ(PROXY, request->get_proxy_address());
  EXPECT_EQ(TIMEOUT_MSEC, request->get_timeout_msec());
  EXPECT_EQ(HTTP_HEADERS, request->get_http_headers());
  EXPECT_EQ(POST_DATA, request->get_post_data());
  EXPECT_EQ(CERTIFICATE_AUTHORITY_BUNDLE_FILE, request->get_ssl_settings().certificate_authority_bundle_file);
  EXPECT_EQ(CLIENT_PRIVATE_KEY_FILE, request->get_ssl_settings().client_private_key_file);
  EXPECT_EQ(CLIENT_CERTIFICATE_FILE, request->get_ssl_settings().client_certificate_file);
}
}  // namespace test
}  // namespace asio_http
