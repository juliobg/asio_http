/**
    asio_http: http client library for boost asio
    Copyright (c) 2017-2019 Julio Becerra Gomez
    See COPYING for license information.
*/

#include <algorithm>
#include <boost/asio.hpp>
#include <iostream>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{
auto find_string_ic(std::string& str_haystack, std::string& str_needle)
{
  auto it =
    std::search(str_haystack.begin(), str_haystack.end(), str_needle.begin(), str_needle.end(), [](char ch1, char ch2) {
      return std::toupper(ch1) == std::toupper(ch2);
    });
  return it;
}

char* mystrinstr(const char* haystack, const char* needle, std::size_t len)
{
  do
  {
    std::size_t l = len;
    const char* h = haystack;
    const char* n = needle;
    while (tolower((unsigned char)*h) == tolower((unsigned char)*n) && *n && l)
    {
      h++;
      n++;
      l--;
    }
    if (*n == 0)
    {
      return (char*)haystack;
    }
    len--;
  } while (*haystack++ && len);
  return nullptr;
}

char* mystrnstr(const char* haystack, const char* needle, size_t len)
{
  int    i;
  size_t needle_len;

  if (0 == (needle_len = strnlen(needle, len)))
    return (char*)haystack;

  for (i = 0; i <= (int)(len - needle_len); i++)
  {
    if ((haystack[0] == needle[0]) && (0 == strncmp(haystack, needle, needle_len)))
      return (char*)haystack;

    haystack++;
  }
  return nullptr;
}

const char server[]    = "HTTP Server";
const char not_found[] = "Content-type: text/html\r\n\r\n<HTML><title>not found</title><body bgcolor=FFFFFF><font "
                         "size=6>404 Not found...</font><BR><BR><HR><small><i>%s</i></small></body></html>\n\r";
const char range_msg[] =
  "Content-type: text/html\r\n\r\n<HTML><title>Requested Range Not Satisfiable</title><body "
  "bgcolor=FFFFFF><font size=6>416 Requested Range Not Satisfiable</font><BR><BR></body></html>\n\r";
}  // namespace

namespace asio_http
{
namespace test_server
{
class web_client : public std::enable_shared_from_this<web_client>
{
public:
  web_client(boost::asio::io_context&                                                                 context,
             boost::asio::ip::tcp::socket&&                                                           socket,
             std::shared_ptr<std::map<std::string, std::function<void(std::shared_ptr<web_client>)>>> handlers_map)
      : m_read_buffer(1024)
      , m_output_buffer()
      , m_write_buffer()
      , m_socket(std::move(socket))
      , m_header_size(0)
      , m_content_size(0)
      , m_requested_range(0)
      , m_handlers_map(handlers_map)
      , m_can_close(false)
      , m_close_connection(false)
      , m_timer(context)
  {
  }
  std::vector<char>                                                                        m_read_buffer;
  std::vector<char>                                                                        m_request_buffer;
  std::vector<char>                                                                        m_response_buffer;
  std::vector<char>                                                                        m_output_buffer;
  std::vector<char>                                                                        m_write_buffer;
  std::string                                                                              m_http_head;
  boost::asio::ip::tcp::socket                                                             m_socket;
  std::size_t                                                                              m_header_size;
  std::size_t                                                                              m_content_size;
  std::size_t                                                                              m_requested_range;
  std::map<std::string, std::string>                                                       m_headers;
  std::shared_ptr<std::map<std::string, std::function<void(std::shared_ptr<web_client>)>>> m_handlers_map;
  bool                                                                                     m_can_close;
  bool                                                                                     m_close_connection;
  boost::asio::deadline_timer                                                              m_timer;

  void start_reading()
  {
    auto shared = shared_from_this();

    m_socket.async_read_some(boost::asio::buffer(m_read_buffer),
                             [shared](auto error_code, auto size) mutable { shared->read_client(error_code, size); });
  }

  void read_client(const boost::system::error_code& error, size_t size)
  {
    const char* tmp;

    if ((boost::asio::error::eof != error) && (boost::asio::error::connection_reset != error))
    {
      m_request_buffer.insert(std::end(m_request_buffer), std::begin(m_read_buffer), std::begin(m_read_buffer) + size);

      if (m_header_size == 0)
      {
        if ((tmp = mystrnstr(m_request_buffer.data(), "\r\n\r\n", m_request_buffer.size())))
        {
          m_header_size = (tmp - m_request_buffer.data());
          m_http_head.assign(m_request_buffer.begin(), m_request_buffer.end());
        }
      }
      if (m_header_size != 0)
      {
        auto dataSize = m_request_buffer.size() - m_header_size;
        if (m_content_size == 0)
        {
          const std::string contentLength = get_header("Content-Length");
          if (!contentLength.empty())
          {
            m_content_size = std::stol(contentLength);
          }
        }

        if (m_content_size == dataSize - 4)
        {
          process_client();
        }
        else
        {
          start_reading();
        }
      }
      else
      {
        start_reading();
      }
    }
  }

  std::string get_header(const std::string& name)
  {
    std::string value;

    const auto it = m_headers.find(name);
    if (it != m_headers.end())
    {
      value = it->second;
    }
    else
    {
      std::string header = name + ": ";
      std::string new_line("\r\n");
      auto        it_begin = find_string_ic(m_http_head, header);
      if (it_begin != m_http_head.end())
      {
        auto it_end     = std::search(it_begin, m_http_head.end(), new_line.begin(), new_line.end());
        value           = std::string(it_begin + header.length(), it_end);
        m_headers[name] = value;
      }
    }

    return value;
  }

  std::string get_method()
  {
    std::string value;

    char* end = strstr(m_request_buffer.data(), " ");
    if (end != nullptr)
    {
      value = std::string(m_request_buffer.data(), end - m_request_buffer.data());
    }

    return value;
  }

  std::string get_request_resource()
  {
    std::string value;
    char*       tmp1 = mystrnstr(m_request_buffer.data(), "/", m_request_buffer.size());
    char*       tmp2 = mystrnstr(tmp1, "?", m_request_buffer.size() - (&(*m_request_buffer.begin()) - tmp1));
    char*       tmp3 = mystrnstr(tmp1, " HTTP", m_request_buffer.size() - (&(*m_request_buffer.begin()) - tmp1));

    if (tmp1 == nullptr || tmp3 == nullptr)
    {
      return value;
    }
    if (tmp2 == nullptr || tmp2 > tmp3)
    {
      tmp2 = tmp3;
    }
    std::string src(tmp1, tmp2 - tmp1);
    char        ch;
    int         i, ii;
    for (i = 0; i < src.length(); i++)
    {
      if (int(src[i]) == 37)
      {
        sscanf(src.substr(i + 1, 2).c_str(), "%x", &ii);
        ch = static_cast<char>(ii);
        value += ch;
        i += 2;
      }
      else
      {
        value += src[i];
      }
    }
    return value;
  }

  std::vector<char> get_post_data()
  {
    char* begin = mystrnstr(m_request_buffer.data(), "\r\n\r\n", m_request_buffer.size());
    return std::vector<char>(begin + 4, &(*(--m_request_buffer.end())) + 1);
  }

  void response_printf(const char* fmt, ...)
  {
    va_list args;

    va_start(args, fmt);

    auto              len = vsnprintf(nullptr, 0, fmt, args);
    std::vector<char> formattedString(len + 1);
    vsnprintf(formattedString.data(), len + 1, fmt, args);

    m_response_buffer.insert(std::end(m_response_buffer), std::begin(formattedString), --std::end(formattedString));

    va_end(args);
  }

  void process_client()
  {
    auto it = m_handlers_map->find(get_request_resource());
    if (it != m_handlers_map->end())
    {
      it->second(shared_from_this());
      if (!m_response_buffer.empty())
      {
        send_http_reply();
      }
      else
      {
        m_timer.expires_from_now(boost::posix_time::millisec(120000));
        m_timer.async_wait([ptr = shared_from_this()](auto&& ec) {
          if (!ec)
          {
            ptr->process_client();
          }
        });
      }
    }
    else
    {
      web_client_writef("HTTP/1.1 404 Not Found\r\n");
      web_client_writef("Server: %s\r\n", "Test server");
      web_client_writef(not_found, server);
    }
  }

  void send_http_reply()
  {
    const auto writeLength = m_response_buffer.size();

    std::size_t wheaderSize;

    char* tmp = mystrnstr(m_response_buffer.data(), "\r\n\r\n", m_response_buffer.size());
    if (tmp != nullptr)
    {
      wheaderSize = tmp - &(*m_response_buffer.begin()) + 4;
    }
    else
    {
      wheaderSize = 0;
    }

    if (m_requested_range > writeLength - wheaderSize && m_requested_range > 0)
    {
      web_client_writef("HTTP/1.1 416 Requested Range Not Satisfiable\r\n");
      web_client_writef("Server: %s\r\n", server);
      web_client_writef(range_msg, server);
    }

    if (m_requested_range > 0)
    {
      web_client_writef("HTTP/1.1 206 Partial Content\r\n");
    }
    else if (mystrnstr(m_response_buffer.data(), "Location", m_response_buffer.size()) != nullptr)
    {
      web_client_writef("HTTP/1.1 301 Moved Permanently\r\n");
    }
    else
    {
      web_client_writef("HTTP/1.1 200 OK\r\n");
    }

    web_client_writef("Server: %s\r\n", server);

    web_client_writef("Accept-Ranges: bytes\r\n");
    if (((writeLength - wheaderSize) - m_requested_range) > 0)
      web_client_writef("Content-length: %zu\r\n", (writeLength - wheaderSize) - m_requested_range);

    m_output_buffer.insert(std::end(m_output_buffer), std::begin(m_response_buffer), std::end(m_response_buffer));
    client_send();
    m_can_close = true;
  }

  void web_client_writef(const char* fmt, ...)
  {
    va_list args;
    char    buf[1024];
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);

    m_output_buffer.insert(std::end(m_output_buffer), buf, buf + strlen(buf));

    client_send();
  }

  void client_send()
  {
    if (m_write_buffer.empty() && !m_output_buffer.empty())
    {
      std::swap(m_output_buffer, m_write_buffer);

      auto shared = shared_from_this();

      m_socket.async_write_some(boost::asio::buffer(m_write_buffer), [shared](auto error_code, auto size) mutable {
        shared->write_client(error_code, size);
      });
    }
    else if (m_can_close)
    {
      if (m_close_connection)
      {
        m_socket.close();
      }
      else
      {
        client_data_cleanup();
        start_reading();
      }
    }
  }

  void client_data_cleanup()
  {
    m_can_close        = false;
    m_close_connection = false;
    m_response_buffer.clear();
    m_output_buffer.clear();
    m_write_buffer.clear();
    m_request_buffer.clear();
    m_http_head.clear();
    m_header_size     = 0;
    m_content_size    = 0;
    m_requested_range = 0;
    m_timer.cancel();
  }

  void write_client(const boost::system::error_code& error, size_t size)
  {
    m_write_buffer.erase(m_write_buffer.begin(), m_write_buffer.begin() + size);

    if (!m_write_buffer.empty())
    {
      auto shared = shared_from_this();

      m_socket.async_write_some(boost::asio::buffer(m_write_buffer), [shared](auto error_code, auto size) mutable {
        shared->write_client(error_code, size);
      });
    }
    else
    {
      client_send();
    }
  }
};

class web_server
{
public:
  web_server(boost::asio::io_context&                                                io_context,
             const std::string&                                                      address,
             const std::uint16_t                                                     port,
             std::map<std::string, std::function<void(std::shared_ptr<web_client>)>> handlers)
      : m_io_context(io_context)
      , m_endpoint(boost::asio::ip::address::from_string(address), port)
      , m_acceptor(io_context, m_endpoint)
      , m_handlers_map(
          std::make_shared<std::map<std::string, std::function<void(std::shared_ptr<web_client>)>>>(handlers))
  {
    m_acceptor.listen();

    start_accept();
  }

private:
  void start_accept()
  {
    m_acceptor.async_accept(
      [this](auto error_code, auto socket) { this->handle_accept(error_code, std::move(socket)); });
  }
  void handle_accept(const boost::system::error_code& error_code, boost::asio::ip::tcp::socket socket)
  {
    const auto newClient = std::make_shared<web_client>(m_io_context, std::move(socket), m_handlers_map);
    newClient->start_reading();
    start_accept();
  }

  boost::asio::io_context&                                                                 m_io_context;
  boost::asio::ip::tcp::endpoint                                                           m_endpoint;
  boost::asio::ip::tcp::acceptor                                                           m_acceptor;
  std::vector<std::shared_ptr<web_client>>                                                 m_clients;
  std::shared_ptr<std::map<std::string, std::function<void(std::shared_ptr<web_client>)>>> m_handlers_map;
};
}  // namespace test_server
}  // namespace asio_http
