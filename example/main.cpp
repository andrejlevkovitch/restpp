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

class request_t : public http::request {
public:
  request_t(http::request req, http::url::args path_args)
      : http::request{std::move(req)}
      , path_args_{std::move(path_args)} {
  }

  http::url::args path_args() const {
    return path_args_;
  }

private:
  http::url::args path_args_;
};

class echo_service final : public restpp::service {
public:
  using req_hanlder   = std::function<void(request_t, OUTPUT http::response &)>;
  using path_handler  = std::tuple<http::url::path::signature, req_hanlder>;
  using path_handlers = std::list<path_handler>;
  using handlers      = std::unordered_map<http::verb, path_handlers>;

  template <typename Method>
  req_hanlder bind_handler(Method method) {
    return std::bind(
        [method, this](request_t req, http::response &res) {
          try {
            (this->*method)(std::move(req), res);
          } catch (std::exception &e) {
            LOG_ERROR(e.what());

            // set error to response
            res = http::response{};
            res.result(http::status::internal_server_error);
          }
        },
        std::placeholders::_1,
        std::placeholders::_2);
  }

  echo_service() {
    using namespace http::literals;


    handlers_[http::verb::get].emplace_back("/",
                                            bind_handler(&echo_service::root));
    handlers_[http::verb::get].emplace_back("/hello/<who>",
                                            bind_handler(&echo_service::hello));
    handlers_[http::verb::get].emplace_back(http::url::path::signature::any(),
                                            bind_handler(&echo_service::echo));

    handlers_[http::verb::post].emplace_back(
        "/data",
        bind_handler(&echo_service::echo_body));
  }

  void handle(http::request req, OUTPUT http::response &res) override {
    using namespace http::literals;

    http::url::path path{req.relative()};

    http::url::path::signature::args path_args;
    for (const auto &[signature, handler] : handlers_[req.method()]) {
      if (signature.match(path, path_args)) {
        handler(request_t{std::move(req), std::move(path_args)}, res);
        return;
      }
    }

    res.result(http::status::not_found);
    return;
  }

  void root(request_t, OUTPUT http::response &res) {
    res.set(http::header::content_type, "text/plain");
    res.body() = "print something to url path";
  }

  void echo(request_t req, OUTPUT http::response &res) {
    res.set(http::header::content_type, "application/x-www-form-urlencoded");
    res.body() = req.relative();
  }

  void hello(request_t req, OUTPUT http::response &res) {
    res.set(http::header::content_type, "text/plain");
    res.body() = "who is " + req.path_args()["who"] + "?";
  }

  void echo_body(request_t req, OUTPUT http::response &res) {
    // at first we need read body
    req.read_body();
    std::string body = std::move(req.body());

    if (body.empty()) {
      body = "print something to request body";
    }


    res.set(http::header::content_type, req[http::header::content_type]);
    res.set(http::header::content_length, body.size());

    res.body() = std::move(body);
  }

private:
  handlers handlers_;
};

class echo_serviceFactory final : public restpp::service_factory {
public:
  restpp::service_ptr make_service() {
    return std::make_shared<echo_service>();
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
  server_builder.add_service(std::make_shared<echo_serviceFactory>(), "/");


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
