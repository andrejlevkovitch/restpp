// request.hpp

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <restpp/misc.hpp>


namespace restpp {
class session;
}

namespace restpp::http {
class request
    : public boost::beast::http::request<boost::beast::http::string_body> {
  friend session;
  using message = boost::beast::http::request<boost::beast::http::string_body>;
  using read_body_callback = std::function<message::body_type::value_type()>;

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
   * \warning you need call it only once
   */
  void read_body() {
    body() = read_body_callback_();
  }

  std::string_view target() const noexcept {
    return misc::string_view_cast(message::target());
  }

  std::string_view relative() const noexcept {
    return misc::string_view_cast(this->target().substr(root_.size()));
  }

  std::string_view root() const noexcept {
    return root_;
  }

protected:
  using message::target;

private:
  read_body_callback read_body_callback_;
  std::string        root_;
};
} // namespace restpp::http
