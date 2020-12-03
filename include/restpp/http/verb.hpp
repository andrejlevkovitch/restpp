// verb.hpp

#pragma once

#include <boost/beast/http/verb.hpp>
#include <restpp/misc.hpp>


namespace restpp::http {
using verb = boost::beast::http::verb;

inline verb string_to_verb(std::string_view method) {
  return boost::beast::http::string_to_verb(
      boost::string_view{method.data(), method.size()});
}


namespace literals {
inline verb operator""_verb(const char *str, size_t len) {
  return string_to_verb(std::string_view{str, len});
}
} // namespace literals
} // namespace restpp::http
