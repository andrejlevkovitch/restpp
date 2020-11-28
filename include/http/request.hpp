// request.hpp

#pragma once

#include "misc.hpp"
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>


namespace http {
class session;

class request
    : public boost::beast::http::request<boost::beast::http::string_body> {
  friend session;

public:
  using boost::beast::http::request<boost::beast::http::string_body>::request;

  std::string_view relative() const noexcept {
    return misc::string_view_cast(this->target().substr(root_.size()));
  }

  std::string_view root() const noexcept {
    return root_;
  }

private:
  std::string root_;
};
} // namespace http
