/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
 */

#ifndef ASIO_HTTP_TUPLE_PTR_H
#define ASIO_HTTP_TUPLE_PTR_H
#include <atomic>
#include <memory>
#include <tuple>
#include <type_traits>

namespace asio_http
{
namespace internal
{
template<typename... As, typename... Bs, template<typename...> class T1, template<typename...> class T2, typename... Ps>
auto zip(T1<As...> t1, T2<Bs...> t2, Ps... ps) -> std::tuple<std::pair<As, Bs>...>
{
  if constexpr (sizeof...(ps) >= std::tuple_size<T1<As...>>::value)
    return std::make_tuple(ps...);
  else
    return zip(t1, t2, ps..., std::make_pair(std::get<sizeof...(ps)>(t1), std::get<sizeof...(ps)>(t2)));
}

// Shared count code based on libc++
class shared_count
{
  shared_count(const shared_count&) = delete;
  shared_count& operator=(const shared_count&) = delete;

protected:
  std::atomic<long> shared_owners;
  virtual ~shared_count() {}

private:
  virtual void on_zero_shared() = 0;

public:
  explicit shared_count(long refs = 0)
      : shared_owners(refs)
  {
  }

  void add_shared() { shared_owners.fetch_add(1, std::memory_order_relaxed); }
  bool release_shared()
  {
    if (shared_owners.fetch_sub(1, std::memory_order_acq_rel) == 0)
    {
      on_zero_shared();
      return true;
    }
    return false;
  }

  long use_count() const { return shared_owners.load(std::memory_order_relaxed) + 1; }
};

class shared_weak_count : private shared_count
{
  std::atomic<long> shared_weak_owners;

public:
  explicit shared_weak_count(long refs = 0)
      : shared_count(refs)
      , shared_weak_owners(refs)
  {
  }

protected:
  virtual ~shared_weak_count() {}

public:
  void add_shared() { shared_count::add_shared(); }
  void add_weak() { shared_weak_owners.fetch_add(1, std::memory_order_relaxed); }
  void release_shared()
  {
    if (shared_count::release_shared())
    {
      release_weak();
    }
  }
  void release_weak()
  {
    if (shared_weak_owners.load(std::memory_order_acquire) == 0)
    {
      // no need to do this store, because we are about
      // to destroy everything.
      // shared_weak_owners.store(-1, std::memory_order_release);
      on_zero_shared_weak();
    }
    else if (shared_weak_owners.fetch_sub(1, std::memory_order_acq_rel) == 0)
    {
      on_zero_shared_weak();
    }
  }
  long               use_count() const { return shared_count::use_count(); }
  shared_weak_count* lock();

private:
  virtual void on_zero_shared_weak() = 0;
};

template<class Tp, class Dp, template<typename> class Alloc>
class shared_ptr_pointer : public shared_weak_count
{
  std::pair<Tp, Dp> data;

public:
  shared_ptr_pointer(Tp p, Dp d)
      : data(std::pair<Tp, Dp>(p, std::move(d)))
  {
  }

private:
  virtual void on_zero_shared()
  {
    auto zipped = zip(data.first, data.second);
    std::apply(
      [](auto&&... args) {
        ((args.second(args.first)), ...);
        ((std::destroy_at(std::addressof(args.second))), ...);
      },
      zip(data.first, data.second));
  }
  virtual void on_zero_shared_weak()
  {
    using Al = Alloc<shared_ptr_pointer>;

    typedef std::allocator_traits<Al>                      ATraits;
    typedef std::pointer_traits<typename ATraits::pointer> PTraits;

    Al a{};
    a.deallocate(PTraits::pointer_to(*this), 1);
  }
};

template<class Tp>
class shared_tuple_base;
template<class... Tp>
class tuple_ptr;

template<typename... Tp>
using tuple_element_ptr = tuple_ptr<Tp...>;

template<typename... Args>
class tuple_ptr
{
  template<typename... Y>
  friend class tuple_ptr;

public:
  template<int N, typename... Ts>
  using NthTypeOf    = typename std::tuple_element<N, std::tuple<Ts...>>::type;
  using element_type = std::tuple<Args*...>;

private:
  element_type       ptrs;
  shared_weak_count* cntrl;

public:
  template<typename Y>
  using tuple_ptr_default_allocator = std::allocator<Y>;
  struct nat
  {
    int for_bool;
  };

  constexpr tuple_ptr() noexcept
      : ptrs{}
      , cntrl(nullptr)
  {
  }

  template<typename... Y,
           bool = std::is_same_v<std::enable_if_t<std::is_convertible_v<std::tuple<Y*...>, element_type>, nat>, nat>>
  explicit tuple_ptr(Y*... p)
      : ptrs(p...)
  {
    typedef shared_ptr_pointer<std::tuple<Y*...>, std::tuple<std::default_delete<Y>...>, tuple_ptr_default_allocator>
      CntrlBlk;
    cntrl = new CntrlBlk(std::make_tuple(p...), std::make_tuple(std::default_delete<Y>()...));
    (enable_weak_this(p, p), ...);
  }

  tuple_ptr(const tuple_ptr& r)
      : ptrs(r.ptrs)
      , cntrl(r.cntrl)
  {
    if (cntrl)
    {
      cntrl->add_shared();
    }
  }

  template<typename... Y,
           bool = std::is_same_v<std::enable_if_t<std::is_convertible_v<std::tuple<Y*...>, element_type>, nat>, nat>>
  tuple_ptr(const tuple_ptr<Y...>& r)
      : ptrs(r.ptrs)
      , cntrl(r.cntrl)
  {
    if (cntrl)
    {
      cntrl->add_shared();
    }
  }

  template<typename... Y,
           bool = std::is_same_v<std::enable_if_t<std::is_convertible_v<std::tuple<Y*...>, element_type>, nat>, nat>>
  tuple_ptr(tuple_ptr<Y...>&& r)
      : ptrs(r.ptrs)
      , cntrl(r.cntrl)
  {
    std::apply([](auto&... ptr) { ((ptr = nullptr), ...); }, r.ptrs);
    r.cntrl = nullptr;
  }

  ~tuple_ptr()
  {
    if (cntrl)
    {
      cntrl->release_shared();
    }
  }

  tuple_ptr<Args...>& operator=(const tuple_ptr& r)
  {
    tuple_ptr(r).swap(*this);
    return *this;
  }

  template<typename... Y,
           bool = std::is_same_v<std::enable_if_t<std::is_convertible_v<std::tuple<Y*...>, element_type>, nat>, nat>>
  tuple_ptr<Args...>& operator=(const tuple_ptr<Y...>& r)
  {
    tuple_ptr(r).swap(*this);
    return *this;
  }

  void swap(tuple_ptr& r)
  {
    std::swap(ptrs, r.ptrs);
    std::swap(cntrl, r.cntrl);
  }

  auto operator-> () const noexcept
  {
    static_assert(1 == sizeof...(Args), "There are more than one element");
    return std::get<0>(ptrs);
  }

  void reset() { tuple_ptr().swap(*this); }

  operator bool() const { return cntrl != nullptr; }

  template<typename T>
  tuple_ptr(T* ptr_, shared_weak_count* cntrl_)
      : ptrs(ptr_)
      , cntrl(cntrl_)
  {
    cntrl->add_shared();
  }

  template<std::size_t N>
  auto get() const
  {
    static_assert(N < sizeof...(Args), "Trying to get unexisting element");
    return tuple_ptr<std::remove_pointer_t<std::tuple_element_t<N, element_type>>>(std::get<N>(ptrs), cntrl);
  }

  auto get() const { return std::get<0>(ptrs); }

  template<class Yp, class OrigPtr>
  typename std::enable_if<std::is_convertible_v<OrigPtr*, const shared_tuple_base<Yp>*>, void>::type
  enable_weak_this(const shared_tuple_base<Yp>* e, OrigPtr* ptr)
  {
    typedef typename std::remove_cv<Yp>::type RawYp;
    if (e && (e->cntrl == nullptr || e->cntrl->use_count() == 0))
    {
      e->cntrl = cntrl;
      e->ptr   = const_cast<RawYp*>(static_cast<const Yp*>(ptr));
      cntrl->add_weak();
    }
  }

  void enable_weak_this(...) {}
};

class bad_ptr : public std::exception
{
public:
  bad_ptr() noexcept {}
};

template<class Tp>
class shared_tuple_base
{
  mutable shared_weak_count* cntrl;
  mutable Tp*                ptr;

protected:
  shared_tuple_base()
      : cntrl(nullptr)
      , ptr(nullptr)
  {
  }
  shared_tuple_base& operator=(shared_tuple_base const&) { return *this; }
  ~shared_tuple_base()
  {
    if (cntrl != nullptr)
    {
      cntrl->release_weak();
    }
  }

  shared_tuple_base(shared_tuple_base const& other)
  {
    cntrl = other.cntrl;
    if (cntrl != nullptr)
    {
      cntrl->add_weak();
    }
  }

public:
  tuple_element_ptr<Tp> shared_from_this()
  {
    if (cntrl == nullptr || cntrl->use_count() == 0)
    {
      throw bad_ptr{};
    }
    return tuple_element_ptr<Tp>(ptr, cntrl);
  }
  tuple_element_ptr<Tp const> shared_from_this() const
  {
    if (cntrl == nullptr || cntrl->use_count() == 0)
    {
      throw bad_ptr{};
    }
    return tuple_element_ptr<const Tp>(ptr, cntrl);
  }

  template<class... Args>
  friend class tuple_ptr;
};

template<class... Tp, class... Up>
inline bool operator==(const tuple_ptr<Tp...>& x, const tuple_ptr<Up...>& y) noexcept
{
  return x.get() == y.get();
}

template<class... Tp, class... Up>
inline bool operator<(const tuple_ptr<Tp...>& x, const tuple_ptr<Up...>& y) noexcept
{
  return x.get() < y.get();
}

template<class... Tp, class... Up>
inline bool operator!=(const tuple_ptr<Tp...>& x, const tuple_ptr<Up...>& y) noexcept
{
  return !(x == y);
}

template<class... Tp, class... Up>
inline bool operator<=(const tuple_ptr<Tp...>& x, const tuple_ptr<Up...>& y) noexcept
{
  return x < y || x == y;
}

template<class... Tp, class... Up>
inline bool operator>(const tuple_ptr<Tp...>& x, const tuple_ptr<Up...>& y) noexcept
{
  return !(x <= y);
}

template<class... Tp, class... Up>
inline bool operator>=(const tuple_ptr<Tp...>& x, const tuple_ptr<Up...>& y) noexcept
{
  return !(x < y);
}
}  // namespace internal
}  // namespace asio_http
#endif
