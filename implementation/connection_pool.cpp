/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/
#include "asio_http/internal/connection_pool.h"

#include "asio_http/internal/http_client_connection.h"
#include "asio_http/internal/http_content.h"
#include "asio_http/internal/http_stack_shared.h"
#include "asio_http/internal/encoding.h"
#include "asio_http/internal/socket.h"
#include "asio_http/internal/tuple_ptr.h"
#include "asio_http/url.h"

#include "loguru.hpp"

#include <cinttypes>
#include <memory>
#include <tuple>
#include <utility>

namespace asio_http
{
namespace internal
{
template<std::size_t N, typename Ls>
using transport = tcp_socket<N, Ls, boost::asio::strand<boost::asio::io_context::executor_type>>;
template<std::size_t N, typename Ls>
using ssl_transport = ssl_socket<N, Ls, boost::asio::strand<boost::asio::io_context::executor_type>>;

template<template<std::size_t N, typename> class... Ls>
struct template_to_tuple
{
  template<typename P, std::size_t... I>
  using type = std::tuple<Ls<I, P>...>;
};

template<template<typename, std::size_t...> typename Ls, typename T>
struct add_indices;

template<template<typename, std::size_t...> typename Ls, std::size_t... I>
struct add_indices<Ls, std::index_sequence<I...>>
{
  template<typename P>
  using type = Ls<P, I...>;
};

template<template<typename> typename Ls>
struct stack_type_list
{
  using types = Ls<stack_type_list>;
  template<std::size_t N>
  using type = typename std::tuple_element<N, types>::type;
};

template<std::size_t X, std::size_t... Xs>
struct drop_first : std::index_sequence<Xs...>
{
};

template<typename T, typename U>
struct drop_last_impl;

template<std::size_t... I, std::size_t... J>
struct drop_last_impl<std::index_sequence<I...>, std::index_sequence<J...>>
    : std::index_sequence<std::tuple_element<I, std::tuple<std::integral_constant<std::size_t, J>...>>::type::value...>
{
};

template<std::size_t... Xs>
struct drop_last : drop_last_impl<std::make_index_sequence<sizeof...(Xs) - 1>, std::index_sequence<Xs...>>
{
};

template<typename... Ls, template<typename...> typename T, typename Args, std::size_t... I>
auto allocate_layers(T<Ls...>*, Args&& args, std::index_sequence<I...>)
{
  std::tuple<std::unique_ptr<Ls>...> ptrs;

  (std::apply(
     [&ptrs](auto&&... args) {
       std::get<I>(ptrs).reset(new
                               typename std::tuple_element<I, T<Ls...>>::type( std::forward<decltype(args)>(args)... ));
     },
     std::get<I>(std::forward<Args>(args))),
   ...);

  connect_layers(drop_first<I...>(), drop_last<I...>(), ptrs);

  auto t = std::apply([](auto&&... args) { return tuple_ptr<Ls...>((args.get())...); }, ptrs);

  ((std::get<I>(ptrs).release()), ...);

  return t;
}

template<std::size_t... I, std::size_t... J, typename T>
void connect_layers(std::index_sequence<I...>, std::index_sequence<J...>, T& tuple)
{
  ((std::get<J>(tuple)->lower_layer = std::get<J + 1>(tuple).get()), ...);
  ((std::get<I>(tuple)->upper_layer = std::get<I - 1>(tuple).get()), ...);
}

template<template<std::size_t N, typename P> class... Ls, typename... Ts>
auto make_shared_stack(Ts... args)
{
  using seq = std::make_index_sequence<sizeof...(Ls)>;

  using templateindexedtuple = add_indices<template_to_tuple<Ls...>::template type, seq>;

  using type_list = stack_type_list<templateindexedtuple::template type>;

  typename type_list::types* dummy = nullptr;

  return allocate_layers(dummy, std::make_tuple(args...), seq());
}

http_stack connection_pool::get_connection(const url& url, const ssl_settings& ssl)
{
  http_stack handle;

  auto host = std::make_pair(url.host, url.port);

  if (m_connection_pool[host].empty())
  {
    handle = create_stack(url, ssl);
    m_allocations++;
  }
  else
  {
    handle = m_connection_pool[host].top();
    m_connection_pool[host].pop();
  }

  return handle;
}

http_stack connection_pool::create_stack(const url& url, const ssl_settings& ssl)
{
  auto shared_data = std::make_shared<http_stack_shared>(m_context);
  auto host        = std::make_pair(url.host, url.port);

  if (url.protocol == "https")
  {
    auto stack = make_shared_stack<http_content, encoding, http_client_connection, ssl_transport>(
      std::make_tuple(shared_data, std::reference_wrapper(m_context), host),
      std::make_tuple(),
      std::make_tuple(shared_data),
      std::make_tuple(shared_data, std::reference_wrapper(m_context), url.host, ssl));
    return stack.get<0>();
  }
  else
  {
    auto stack = make_shared_stack<http_content, encoding, http_client_connection, transport>(
      std::make_tuple(shared_data, std::reference_wrapper(m_context), host),
      std::make_tuple(),
      std::make_tuple(shared_data),
      std::make_tuple(shared_data, std::reference_wrapper(m_context)));
    return stack.get<0>();
  }
}

void connection_pool::release_connection(http_stack handle, bool clean_up)
{
  auto http_layer = handle;

  const auto host = http_layer->get_host_and_port();

  // Throw away handle in case of error and clean all others
  if (clean_up)
  {
    auto new_stack = std::stack<http_stack>();
    m_connection_pool[host].swap(new_stack);
  }
  else
  {
    m_connection_pool[host].push(handle);
  }
}

connection_pool::~connection_pool()
{
  DLOG_F(INFO, "Destroyed connection pool after allocations: %" PRIu64, m_allocations);
}
}  // namespace internal
}  // namespace asio_http
