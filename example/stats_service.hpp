// stats_service.hpp

#pragma once


#include <restpp/service.hpp>


class stats_service final : public restpp::service {
public:
  void handle(restpp::http::request &req, restpp::http::response &res) override;

  static void increment(std::string_view target);
  static void increment(const restpp::http::request &req);
};

ADD_DEFAULT_FACTORY(stats_service);
