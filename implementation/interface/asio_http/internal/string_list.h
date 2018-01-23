/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_STRING_LIST_H
#define ASIO_HTTP_STRING_LIST_H

struct curl_slist;

#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace asio_http
{
namespace internal
{
class string_list
{
public:
  string_list()
      : m_curllist(nullptr)
  {
  }

  string_list(const string_list&) = delete;

  operator curl_slist*() { return m_curllist; }

  curl_slist* get() { return m_curllist; }

  string_list& append(const std::string& entry);

  string_list& append_from_vector(const std::vector<std::string>& entries);

  ~string_list();

  class iterator : public std::iterator<std::input_iterator_tag, char* const>
  {
  public:
    iterator()
        : m_curllist(nullptr)
    {
    }

    explicit iterator(const curl_slist* list)
        : m_curllist(list)
    {
    }

    iterator& operator++();

    bool operator==(const iterator& iterator) const { return m_curllist == iterator.m_curllist; }

    bool operator!=(const iterator& iterator) const { return m_curllist != iterator.m_curllist; }

    char* operator*() const;

  private:
    const curl_slist* m_curllist;
  };

  iterator begin() const { return iterator(m_curllist); }

  iterator end() const { return iterator(); }

private:
  curl_slist* m_curllist;
};
}
}
#endif
