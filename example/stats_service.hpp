// stats_service.hpp

#pragma once


#include <restpp/service.hpp>


class stats_service final : public restpp::service {
public:
  void handle(restpp::http::request &req, restpp::http::response &res) override;

  static void increment(std::string_view target);
  static void increment(const restpp::http::request &req);
};


class stats_service_factory : public restpp::service_factory {
public:
  restpp::service_ptr make_service() override {
    return std::make_shared<stats_service>();
  }
};
