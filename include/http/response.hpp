// response.hpp

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>


namespace http {
class session;

class response
    : public boost::beast::http::response<boost::beast::http::string_body> {
  friend session;

public:
  /**\brief write headers to client separately of response body
   * \warning you can call it only once!
   */
  void write_headers() {
    if (write_headers_callback_ == nullptr) {
      throw std::runtime_error{"second call of write headers callback"};
    }

    write_headers_callback_();
    write_headers_callback_ = nullptr;
  }

  /**\brief write response body
   * \warning you can call it only once!
   * \warning you can call it only after calling write_headers!
   * \see write_headers
   */
  void write_body() {
    if (write_headers_callback_ != nullptr) {
      throw std::logic_error{"trying write body before writing headers"};
    }
    if (write_body_callback_ == nullptr) {
      throw std::runtime_error{"second call of write body callback"};
    }

    write_body_callback_();
    write_body_callback_ = nullptr;
  }

private:
  std::function<void()> write_headers_callback_;
  std::function<void()> write_body_callback_;
};
} // namespace http
