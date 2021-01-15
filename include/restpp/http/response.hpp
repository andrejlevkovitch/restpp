// response.hpp

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>


namespace restpp {
class session;
}

namespace restpp::http {
/**\
 * \note you should not use write_headers or write method. Response writes
 * automaticly to client after finishing handling
 */
class response
    : public boost::beast::http::response<boost::beast::http::string_body> {
  friend session;

  using write_headers_callback = std::function<void()>;
  using write_body_callback    = std::function<void()>;

public:
  /**\brief write headers to client separately of response body
   * \warning you need call it only once
   * \note this operation hit performance, so use it only if you need specific
   * response handling
   */
  void write_headers() {
    write_headers_callback_();
  }

  /**\brief write complete response to client. If write_headers was called, then
   * write only body
   * \warning you need call it only once
   * \see write_headers
   * \note you shouldn't call it if all you need that write the response without
   * some post-processing. Use it only if you need to do something after writing
   * the response
   */
  void write() {
    write_callback_();
  }

private:
  write_headers_callback write_headers_callback_;
  write_body_callback    write_callback_;
};
} // namespace restpp::http
