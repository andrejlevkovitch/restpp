// http.hpp

#pragma once

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>


namespace http {
using status = boost::beast::http::status;
using verb   = boost::beast::http::verb;
using header = boost::beast::http::field;
} // namespace http
