// main.cpp

#include "restpp/http.hpp"
#include "restpp/server.hpp"
#include "restpp/service.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/stacktrace.hpp>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <simple_logs/logs.hpp>


#define HELP_FLAG         "help"
#define URL_FLAG          "url"
#define THREAD_COUNT_FLAG "threads"
#define VERBOSE_FLAG      "verbose"
#define VERBOSE_SHORT     "v"

#define DEFAULT_HOST         "localhost"
#define DEFAULT_PORT         8000
#define DEFAULT_THREAD_COUNT 1


void sigsegv_handler([[maybe_unused]] int signal) {
  // XXX note that application must be compiled with -g flag
  boost::stacktrace::stacktrace trace{};
  LOG_FAILURE(boost::stacktrace::to_string(trace));
}


namespace http = restpp::http;
namespace asio = boost::asio;


class stats_service final : public restpp::service {
public:
  void handle(http::request &req, http::response &res) override {
    if (req.method() != http::verb::get && req.relative() != "/") {
      res.result(http::status::not_found);
    }

    auto [lock, stats] = get_stats();

    std::stringstream ss;
    size_t            total = 0;
    for (const auto &[target, count] : stats) {
      ss << target << '\t' << count << std::endl;
      total += count;
    }
    ss << "  total:\t" << total << std::endl;

    res.body() = ss.str();
  }

  static void increment(std::string_view target) {
    auto [lock, stats] = get_stats();
    stats[std::string(target)]++;
  }

  static void increment(const http::request &req) {
    http::url::path absolute_path{http::url::get_path(req.target())};
    stats_service::increment(std::string{absolute_path});
  }

private:
  static std::pair<std::unique_lock<std::mutex>,
                   std::unordered_map<std::string, int> &>
  get_stats() {
    static std::mutex                           mutex_;
    static std::unordered_map<std::string, int> calls_;

    return std::make_pair(std::unique_lock<std::mutex>{mutex_},
                          std::ref(calls_));
  }
};

class stats_service_factory : public restpp::service_factory {
public:
  restpp::service_ptr make_service() override {
    return std::make_shared<stats_service>();
  }
};


class echo_service final : public restpp::service {
public:
  using req_hanlder =
      std::function<void(http::request &req, OUTPUT http::response &)>;
  using path_handler  = std::tuple<http::url::path::signature, req_hanlder>;
  using path_handlers = std::list<path_handler>;
  using handlers      = std::unordered_map<http::verb, path_handlers>;

  template <typename Method>
  req_hanlder bind_handler(Method method) {
    return std::bind(method,
                     this,
                     std::placeholders::_1,
                     std::placeholders::_2);
  }

  echo_service() {
    using namespace http::literals;


    handlers_[http::verb::get].emplace_back("/",
                                            bind_handler(&echo_service::root));
    handlers_[http::verb::get].emplace_back(http::url::path::signature::any(),
                                            bind_handler(&echo_service::echo));

    handlers_[http::verb::post].emplace_back(
        "/",
        bind_handler(&echo_service::echo_body));
  }

  void handle(http::request &req, OUTPUT http::response &res) override {
    using namespace http::literals;

    // at first add path to stats
    stats_service::increment(req);

    // parse path and query
    auto [path_str, query_str] = http::url::split(req.relative());


    // try find needed endpoint
    http::url::path path{path_str};
    http::url::args path_args;
    for (const auto &[signature, handler] : handlers_[req.method()]) {
      if (signature.match(path, path_args)) {
        http::url::args query_args = http::url::query::split(query_str);

        handler(req, res);
        return;
      }
    }

    res.result(http::status::not_found);
    return;
  }

  void exception(const http::request &req,
                 OUTPUT http::response &res,
                 std::exception &       e) noexcept override {
    LOG_ERROR("service %1% at %2% get exception: %3%",
              "echo",
              req.target(),
              e.what());

    service::exception(req, res, e);
  }

  void root([[maybe_unused]] const http::request &req,
            OUTPUT http::response &res) {
    res.set(http::header::content_type, "text/plain");
    res.body() = "print something to url path";
  }

  void echo(http::request &req, OUTPUT http::response &res) {
    res.set(http::header::content_type, "application/x-www-form-urlencoded");
    res.body() = req.relative();
  }

  void echo_body(http::request &req, OUTPUT http::response &res) {
    // at first we need read body
    req.read_body();
    std::string body = std::move(req.body());

    if (body.empty()) {
      body = "print something to request body";
    }


    res.set(http::header::content_type, req[http::header::content_type]);

    res.body() = std::move(body);
  }

private:
  handlers handlers_;
};

class echo_service_factory final : public restpp::service_factory {
public:
  restpp::service_ptr make_service() {
    return std::make_shared<echo_service>();
  }
};


class say_service final : public restpp::service {
public:
  void handle(restpp::http::request & req,
              restpp::http::response &res) override {
    // at first add path to stats
    stats_service::increment(req);


    std::string_view path_str = http::url::get_path(req.relative());


    http::url::path         path{path_str};
    restpp::http::url::args path_args;
    if (http::url::path::signature{"/<what>"}.match(path, path_args)) {
      res.body() = path_args.at("what") + "\n";
    }

    res.result(http::status::not_found);
  }
};

class say_service_factory final : public restpp::service_factory {
public:
  restpp::service_ptr make_service() {
    return std::make_shared<say_service>();
  }
};


int main(int argc, char *argv[]) {
  std::signal(SIGSEGV, sigsegv_handler);


  // describe program options
  namespace po = boost::program_options;
  po::options_description desc{"provide rest api for users managment"};
  // clang-format off
  desc.add_options()
    (HELP_FLAG, "print description")
    (URL_FLAG, po::value<std::string>()->default_value("http://" DEFAULT_HOST ":" + std::to_string(DEFAULT_PORT)), "url for the service")
    (THREAD_COUNT_FLAG, po::value<ushort>()->default_value(DEFAULT_THREAD_COUNT), "count of started threads")
    (VERBOSE_FLAG "," VERBOSE_SHORT, "show more logs");
  // clang-format on

  // parse arguments
  po::variables_map args;
  po::store(po::parse_command_line(argc, argv, desc), args);

  if (args.count(HELP_FLAG) != 0) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  po::notify(args);


  // setup simple logs
  auto infoBackend  = std::make_shared<logs::TextStreamBackend>(std::cout);
  auto infoFrontend = std::make_shared<logs::LightFrontend>();
  infoFrontend->setFilter(logs::Severity::Placeholder == logs::Severity::Info);

  auto errorBackend  = std::make_shared<logs::TextStreamBackend>(std::cerr);
  auto errorFrontend = std::make_shared<logs::LightFrontend>();
  errorFrontend->setFilter(logs::Severity::Placeholder >=
                           logs::Severity::Error);

  LOGGER_ADD_SINK(infoFrontend, infoBackend);
  LOGGER_ADD_SINK(errorFrontend, errorBackend);

  if (args.count(VERBOSE_FLAG)) {
    auto debugBackend  = std::make_shared<logs::TextStreamBackend>(std::cerr);
    auto debugFrontend = std::make_shared<logs::LightFrontend>();
    debugFrontend->setFilter(
        logs::Severity::Placeholder == logs::Severity::Trace ||
        logs::Severity::Placeholder == logs::Severity::Debug ||
        logs::Severity::Placeholder == logs::Severity::Warning ||
        logs::Severity::Placeholder == logs::Severity::Throw);

    LOGGER_ADD_SINK(debugFrontend, debugBackend);
  }


  std::string server_uri   = args[URL_FLAG].as<std::string>();
  ushort      thread_count = args[THREAD_COUNT_FLAG].as<ushort>();

  LOG_INFO("server uri:       %1%", server_uri);
  LOG_INFO("count of threads: %1%", thread_count);


  asio::io_context io_context;

  restpp::server_builder server_builder{io_context};
  server_builder.set_uri(server_uri);
  server_builder.add_service(std::make_shared<echo_service_factory>(), "/echo");
  server_builder.add_service(std::make_shared<say_service_factory>(), "/say");
  server_builder.add_service(std::make_shared<stats_service_factory>(),
                             "/stats");

  // start the server
  LOG_INFO("start server");
  restpp::server server = server_builder.build();
  server.asyncRun();


  asio::signal_set sigset{io_context, SIGINT, SIGTERM};
  sigset.async_wait([&io_context, &server](boost::system::error_code e, int) {
    if (e.failed()) {
      LOG_ERROR(e.message());
    }

    server.stop();
    io_context.stop();
  });


  std::vector<std::thread> threads;
  for (int i = 0; i < thread_count - 1; ++i) {
    threads.emplace_back([&io_context]() {
      io_context.run();
    });
  }

  io_context.run();
  LOG_INFO("exit");

  for (std::thread &thread : threads) {
    thread.join();
  }

  return EXIT_SUCCESS;
}
