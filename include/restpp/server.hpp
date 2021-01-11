// server.hpp

#pragma once

#include <boost/asio/io_context.hpp>
#include <list>
#include <memory>


namespace restpp {
namespace asio = boost::asio;

class server_impl;
class server_builder;
class service_factory;

class server {
  friend server_builder;

public:
  server &async_run();
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

  /**\brief set maximum number of sessions, that server can keep per one time.
   * By default is not limited
   * \param count 0 is special value - means that server count of sessions not
   * limited
   */
  server_builder &set_max_session_count(size_t count);

  server build() const;

private:
  asio::io_context &io_context_;

  std::string          root_;
  service_factory_list service_factories_;

  size_t max_session_count_;
};
} // namespace restpp
