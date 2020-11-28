// response.hpp

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>


namespace http {
using response = boost::beast::http::response<boost::beast::http::string_body>;
} // namespace http
