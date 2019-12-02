/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/tuple_ptr.h"

#include <gtest/gtest.h>

namespace asio_http
{
namespace internal
{
namespace test
{
struct TestClass
{
  int& counter;
  TestClass(int& counter_)
      : counter{ counter_ }
  {
  }
  ~TestClass() { counter++; }
};

struct TestClass2 : public shared_tuple_base<TestClass2>
{
  int& counter;
  TestClass2(int& counter_)
      : counter{ counter_ }
  {
  }
  ~TestClass2() { counter++; }
};

TEST(shared_ptr_test, test0)
{
  int   counter{};
  auto* p1 = new TestClass{ counter };
  auto* p2 = new TestClass{ counter };

  {
    auto shared = tuple_ptr<TestClass, TestClass>{ p1, p2 };
  }

  EXPECT_EQ(counter, 2);
}

TEST(shared_ptr_test, test1)
{
  int   counter{};
  auto* p1 = new TestClass{ counter };
  auto* p2 = new TestClass{ counter };

  auto shared  = tuple_ptr<TestClass, TestClass>{ p1, p2 };
  auto element = shared.get<0>();

  shared.~tuple_ptr();
  EXPECT_EQ(counter, 0);

  element.~tuple_ptr();
  EXPECT_EQ(counter, 2);
}

TEST(shared_ptr_test, test2)
{
  int   counter{};
  auto* p1 = new TestClass{ counter };
  auto* p2 = new TestClass2{ counter };

  auto shared   = tuple_ptr<TestClass, TestClass2>{ p1, p2 };
  auto shared2  = shared;
  auto element  = p2->shared_from_this();
  auto element2 = element;

  shared.~tuple_ptr();
  EXPECT_EQ(counter, 0);

  element.~tuple_ptr();
  EXPECT_EQ(counter, 0);

  element2.~tuple_ptr();
  EXPECT_EQ(counter, 0);

  shared2.~tuple_ptr();
  EXPECT_EQ(counter, 2);
}

}  // namespace test
}  // namespace internal
}  // namespace asio_http
