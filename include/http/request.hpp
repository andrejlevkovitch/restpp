// request.hpp

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <http/misc.hpp>


namespace http {
class session;

class request
    : public boost::beast::http::request<boost::beast::http::string_body> {
  friend session;
  using message = boost::beast::http::request<boost::beast::http::string_body>;

public:
  request(const message &rhs)
      : message{rhs} {
  }

  request(message &&rhs)
      : message{std::move(rhs)} {
  }

  request &operator=(const message &rhs) {
    message::operator=(rhs);
    return *this;
  }

  request &operator=(message &&rhs) {
    message::operator=(std::move(rhs));
    return *this;
  }

  /**\brief synchronously read request body
   * \note that before getting body from request you need call this metod
   * \warning you can call it only once!
   */
  void read_body() {
    if (read_body_callback_ == nullptr) {
      throw std::runtime_error{"second call of read body callback"};
    }

    message::operator   =(read_body_callback_());
    read_body_callback_ = nullptr;
  }

  std::string_view relative() const noexcept {
    return misc::string_view_cast(this->target().substr(root_.size()));
  }

  std::string_view root() const noexcept {
    return root_;
  }

private:
  std::function<request::message()> read_body_callback_;
  std::string                       root_;
};
} // namespace http
