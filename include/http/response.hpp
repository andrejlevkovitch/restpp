// response.hpp

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>


namespace http {
class session;

/**\
 * \note you should not use write_headers or write method. Response writes
 * automaticly to client after finishing handling
 */
class response
    : public boost::beast::http::response<boost::beast::http::string_body> {
  friend session;

public:
  /**\brief write headers to client separately of response body
   * \warning you can call it only once!
   * \note this operation hit performance, so use it only if you need specific
   * response handling
   */
  void write_headers() {
    if (write_headers_callback_ == nullptr) {
      throw std::runtime_error{"second call of write headers callback"};
    }

    write_headers_callback_();
    write_headers_callback_ = nullptr;
  }

  /**\brief write complete response to client. If write_headers was called, then
   * write only body
   * \warning you can call it only once!
   * \warning you can call it only after calling write_headers!
   * \see write_headers
   * \note you shouldn't call it if all you need that write the response without
   * some post-processing. Use it only if you need to do something after writing
   * the response
   */
  void write() {
    if (write_callback_ == nullptr) {
      throw std::runtime_error{"second call of write response callback"};
    }

    write_callback_();
    write_headers_callback_ = nullptr;
    write_callback_         = nullptr;
  }

private:
  std::function<void()> write_headers_callback_;
  std::function<void()> write_callback_;
};
} // namespace http
