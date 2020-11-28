// server.hpp

#pragma once

#include <boost/asio/io_context.hpp>
#include <list>
#include <memory>


namespace asio = boost::asio;

namespace http {
class server_impl;
class server_builder;
class service_factory;

class server {
  friend server_builder;

public:
  server &asyncRun();
  server &stop();

private:
  std::shared_ptr<server_impl> impl_;
};

class server_builder {
  using service_factory_ptr  = std::shared_ptr<service_factory>;
  using service_kvp          = std::pair<std::string, service_factory_ptr>;
  using service_factory_list = std::list<service_kvp>;

public:
  server_builder(asio::io_context &context);

  server_builder &set_uri(std::string_view root);

  server_builder &add_service(service_factory_ptr factory,
                              std::string_view    path = "");

  server build() const;

private:
  asio::io_context &io_context_;

  std::string          root_;
  service_factory_list service_factories_;
};
} // namespace http
