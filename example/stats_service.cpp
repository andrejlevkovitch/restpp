// stats_service.cpp

#include "stats_service.hpp"
#include <mutex>
#include <restpp/http.hpp>
#include <unordered_map>


namespace http = restpp::http;


static std::mutex                           mutex_;
static std::unordered_map<std::string, int> stats_;


void stats_service::handle(http::request &req, http::response &res) {
  if (req.method() != http::verb::get && req.relative() != "/") {
    res.result(http::status::not_found);
  }

  std::lock_guard<std::mutex> lock{mutex_};

  std::stringstream ss;
  size_t            total = 0;
  for (const auto &[target, count] : stats_) {
    ss << target << '\t' << count << std::endl;
    total += count;
  }
  ss << "  total:\t" << total << std::endl;

  res.body() = ss.str();
}

void stats_service::increment(std::string_view target) {
  std::lock_guard<std::mutex> lock{mutex_};

  stats_[std::string(target)]++;
}

void stats_service::increment(const restpp::http::request &req) {
  http::url::path absolute_path{http::url::get_path(req.target())};
  stats_service::increment(std::string{absolute_path});
}
