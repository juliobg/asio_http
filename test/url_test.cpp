/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/url.h"

#include <gtest/gtest.h>

namespace asio_http
{
namespace test
{
namespace
{
const std::string url_string = "https://any.host.com:1234/some/path?and_query";
}
TEST(url_test, construction)
{
  url url(url_string);
  EXPECT_EQ("any.host.com", url.host);
  EXPECT_EQ(1234, url.port);
  EXPECT_EQ("/some/path", url.path);
  EXPECT_EQ("https", url.protocol);
}

TEST(url_test, to_string)
{
  url url(url_string);
  EXPECT_EQ(url_string, url.to_string());
}

TEST(url_test, compare)
{
  url url_a1("https://oneaddress.com");
  url url_a2("https://oneaddress.com");
  EXPECT_TRUE(url_a1 == url_a2);
  EXPECT_FALSE(url_a1 != url_a2);

  url url_b("https://anotheradress.com");
  EXPECT_FALSE(url_a1 == url_b);
  EXPECT_TRUE(url_a1 != url_b);
}
}  // namespace test
}  // namespace asio_http
