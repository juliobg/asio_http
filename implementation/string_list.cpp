/**
    asio_http: wrapper for integrating libcurl with boost.asio applications
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#include "asio_http/internal/string_list.h"

#include <curl/curl.h>
#include <stdexcept>

namespace asio_http
{
namespace internal
{
string_list& string_list::append(const std::string& entry)
{
  m_curllist = curl_slist_append(m_curllist, entry.c_str());
  return *this;
}

string_list& string_list::append_from_vector(const std::vector<std::string>& entries)
{
  for (const std::string& entry : entries)
  {
    append(entry);
  }

  return *this;
}

string_list::~string_list()
{
  if (m_curllist != nullptr)
  {
    curl_slist_free_all(m_curllist);
  }
}

string_list::iterator& string_list::iterator::operator++()
{
  if (m_curllist)
  {
    m_curllist = m_curllist->next;
  }
  return *this;
}

char* string_list::iterator::operator*() const
{
  if (m_curllist == nullptr)
  {
    throw std::out_of_range("string_list::iterator: past-the-end iterator was dereferenced");
  }
  return m_curllist->data;
}
}
}
