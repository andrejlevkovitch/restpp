// server.cpp

#include "restpp/server.hpp"
#include "restpp/http/request.hpp"
#include "restpp/http/response.hpp"
#include "restpp/http/status.hpp"
#include "restpp/http/url.hpp"
#include "restpp/http/verb.hpp"
#include "restpp/service.hpp"
#include <atomic>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/version.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <list>
#include <regex>
#include <simple_logs/logs.hpp>
#include <sstream>
#include <thread>

// XXX must be after <thread>
#include <boost/asio/yield.hpp>


#define WAIT_SESSION_CLOSE_SECONDS 1


namespace restpp {
namespace asio   = boost::asio;
using tcp        = asio::ip::tcp;
using error_code = boost::system::error_code;

namespace beast = boost::beast;


class session final
    : public asio::coroutine
    , public std::enable_shared_from_this<session> {
public:
  using socket         = tcp::socket;
  using self           = std::shared_ptr<session>;
  using request_buffer = beast::flat_buffer;
  using request_parser = beast::http::request_parser<beast::http::string_body>;
  using response_serializer =
      beast::http::response_serializer<beast::http::string_body>;
  using service_list = std::list<std::pair<http::url::path, service_ptr>>;

  using strand = asio::strand<socket::executor_type>;


  session(socket                sock,
          service_list          services,
          std::optional<size_t> req_headers_limit,
          std::optional<size_t> req_body_limit,
          std::optional<size_t> res_buffer_limit) noexcept
      : socket_ {
    std::move(sock)
  }
#if BOOST_ASIO_VERSION >= 101400
  , strand_ {
    asio::make_strand(sock.get_executor())
  }
#else
  , strand_ {
    sock.get_executor()
  }
#endif
  , is_open_{true}, services_{services}, req_headers_limit_{req_headers_limit},
      req_body_limit_{req_body_limit}, res_buffer_limit_{res_buffer_limit} {
    LOG_TRACE("construct session");
  }

  void start() {
    LOG_TRACE("start new session");

    // @see https://github.com/boostorg/beast/issues/2141
    socket_.set_option(tcp::no_delay(true));


    std::stringstream remoteEndpoint;
    remoteEndpoint << socket_.remote_endpoint();


    for (const auto &[key, service] : services_) {
      service->at_session_start(remoteEndpoint.str());
    }


    self  self_ = this->shared_from_this();
    this->operator()(std::move(self_), error_code{}, 0);
  }

  void close() noexcept {
    if (is_open_ == false) {
      LOG_WARNING("session already closed");
      return;
    }

    LOG_TRACE("close session");

    error_code err;

    // cancel all async operation with this socket
    socket_.cancel(err);
    if (err.failed()) {
      LOG_ERROR(err.message());
    }
  }

  /**\note all async operations with socket must be already canceled
   */
  void at_close() noexcept {
    LOG_TRACE("at session close");

    is_open_ = false;

    error_code err;
    socket_.shutdown(socket::shutdown_both, err);
    if (err.failed()) {
      LOG_ERROR(err.message());
    }

    socket_.close(err);
    if (err.failed()) {
      LOG_ERROR(err.message());
    }

    for (const auto &[key, service] : services_) {
      service->at_session_close();
    }
  }

  bool is_open() const noexcept {
    return is_open_;
  }

private:
  void operator()(self self_, error_code err, size_t transfered) noexcept {
    if (err.failed()) {
      if (err == asio::error::eof || err == beast::http::error::end_of_stream) {
        LOG_DEBUG("client close connection");
      } else if (err == asio::error::operation_aborted) {
        LOG_DEBUG("session canceled");
      } else {
        LOG_ERROR(err.message());
      }

      this->at_close();

      LOG_TRACE("end of session coroutine");
      return;
    }

    reenter(this) {
      for (;;) {
        // XXX every request must be handled by new parser instance
        req_parser_ = std::make_unique<request_parser>();

        if (req_headers_limit_.has_value()) {
          req_parser_->header_limit(req_headers_limit_.value());
        }
        if (req_body_limit_.has_value()) {
          req_parser_->body_limit(req_body_limit_.value());
        }


        LOG_TRACE("try read request headers");

        yield beast::http::async_read_header(
            socket_,
            req_buffer_,
            *req_parser_,
            asio::bind_executor(strand_,
                                std::bind(&session::operator(),
                                          this,
                                          std::move(self_),
                                          std::placeholders::_1,
                                          std::placeholders::_2)));

        LOG_DEBUG("readed headers: %1.3fKb", transfered / 1024.);


        // request handling
        // new response
        res_            = std::make_unique<http::response>();
        res_serializer_ = std::make_unique<response_serializer>(*res_);

        if (res_buffer_limit_.has_value()) {
          res_serializer_->limit(res_buffer_limit_.value());
        }


        {
          boost::system::error_code read_body_err;
          boost::system::error_code write_headers_err;
          boost::system::error_code write_body_err;

          // read body callback
          auto read_body_callback = [this, &read_body_err]() {
            if (req_parser_->is_done() == false) {
              LOG_TRACE("try read request body by callback");

              size_t trans = beast::http::read(socket_,
                                               req_buffer_,
                                               *req_parser_,
                                               read_body_err);
              if (read_body_err.failed()) {
                LOG_ERROR("error while reading body: %1%",
                          read_body_err.message());

                throw boost::system::system_error{read_body_err};
              }

              LOG_DEBUG("readed body: %1.3fKb", trans / 1024.);
            } // else nothing to call


            return req_parser_->get().body();
          };

          // write headers callback
          auto write_headers_callback = [this, &write_headers_err]() {
            if (res_serializer_->is_header_done()) {
              return; // nothing to write
            }

            LOG_TRACE("try write response headers by callback");

            size_t trans = beast::http::write_header(socket_,
                                                     *res_serializer_,
                                                     write_headers_err);
            if (write_headers_err.failed()) {
              LOG_ERROR("error while writing headers: %1%",
                        write_headers_err.message());

              throw boost::system::system_error{write_headers_err};
            }

            LOG_DEBUG("writed headers: %1.3fKb", trans / 1024.);
          };
          auto write_callback = [this, &write_body_err]() {
            if (res_serializer_->is_done()) {
              return; // nothing to write
            }

            LOG_TRACE("try write request by callback");

            size_t trans =
                beast::http::write(socket_, *res_serializer_, write_body_err);
            if (write_body_err.failed()) {
              LOG_ERROR("error while writing response: %1%",
                        write_body_err.message());

              throw boost::system::system_error{write_body_err};
            }

            LOG_DEBUG("writed response: %1.3fKb", trans / 1024.);
          };


          http::request req       = req_parser_->get();
          req.read_body_callback_ = read_body_callback;

          res_->write_headers_callback_ = write_headers_callback;
          res_->write_callback_         = write_callback;


          // set default behaviour of response
          unsigned int version    = req.version();
          bool         keep_alive = req.keep_alive();

          res_->version(version);
          res_->keep_alive(keep_alive);


          // find service for handling
          std::string_view target = misc::string_view_cast(req.target());

          const http::url::path path{http::url::get_path(target)};
          service_ptr           service;
          for (const auto &[root, srv] : services_) {
            if (root.is_base_of(path)) {
              service = srv;

              req.root_ = root;
              break;
            }
          }

          if (service == nullptr) {
            LOG_WARNING("service for %1% not found", target);

            res_->result(http::status::not_found);
            goto PreWriting;
          }


          // handle request
          try {
            LOG_TRACE("try handle request by service");

            service->handle(req, *res_);
          } catch (std::exception &e) {
            LOG_ERROR("catch exception from service: %1%", e.what());

            service->exception(req, *res_, e);
          }

          // at handling service we need check read and write callbacks on
          // errors
          if (read_body_err.failed()) {
            return this->operator()(std::move(self_), read_body_err, 0);
          }
          if (write_headers_err.failed()) {
            return this->operator()(std::move(self_), write_headers_err, 0);
          }
          if (write_body_err.failed()) {
            return this->operator()(std::move(self_), write_body_err, 0);
          }

        PreWriting:
          if (res_serializer_->split() == false) {
            res_->prepare_payload();
          } else if (res_serializer_->is_done()) { // response alredy writed
            goto PostWriting;
          }
        }


        LOG_TRACE("try write request");

        yield beast::http::async_write(
            socket_,
            *res_serializer_,
            asio::bind_executor(strand_,
                                std::bind(&session::operator(),
                                          this,
                                          std::move(self_),
                                          std::placeholders::_1,
                                          std::placeholders::_2)));

        LOG_DEBUG("writed response: %1.3fKb", transfered / 1024.);


      PostWriting:
        // handle eof semantic
        if (res_->need_eof()) {
          this->operator()(std::move(self_),
                           asio::error::make_error_code(asio::error::eof),
                           0);
          return;
        }

        // skip request body if it was set and didn't read
        if (req_parser_->is_done() == false) {
          LOG_TRACE("skip request body");

          yield beast::http::async_read(
              socket_,
              req_buffer_,
              *req_parser_,
              asio::bind_executor(strand_,
                                  std::bind(&session::operator(),
                                            this,
                                            std::move(self_),
                                            std::placeholders::_1,
                                            std::placeholders::_2)));

          LOG_DEBUG("skip request body: %1.3fKb", transfered / 1024.);
        }
      }
    }
  }

private:
  socket           socket_;
  strand           strand_;
  std::atomic_bool is_open_;

  request_buffer                       req_buffer_;
  std::unique_ptr<request_parser>      req_parser_;
  std::unique_ptr<http::response>      res_;
  std::unique_ptr<response_serializer> res_serializer_;

  service_list services_;

  std::optional<size_t> req_headers_limit_;
  std::optional<size_t> req_body_limit_;
  std::optional<size_t> res_buffer_limit_;
};


class server_impl
    : public asio::coroutine
    , public std::enable_shared_from_this<server_impl> {
public:
  using self        = std::shared_ptr<server_impl>;
  using session_ptr = std::shared_ptr<session>;
  using endpoint    = tcp::endpoint;
  using socket      = asio::basic_stream_socket<tcp>;
  using acceptor    = asio::basic_socket_acceptor<tcp>;
  using service_factory_list =
      std::list<std::pair<std::string, service_factory_ptr>>;
  using session_pool = std::list<session_ptr>;


  server_impl(asio::io_context &    io_context,
              endpoint              ep,
              service_factory_list  factories,
              std::optional<size_t> max_session_count,
              std::optional<size_t> req_headers_limit,
              std::optional<size_t> req_body_limit,
              std::optional<size_t> res_buffer_limit)
      : io_context_{io_context}
      , acceptor_{io_context}
      , service_factories_{factories}
      , max_session_count_{max_session_count}
      , req_headers_limit_{req_headers_limit}
      , req_body_limit_{req_body_limit}
      , res_buffer_limit_{res_buffer_limit} {
    LOG_TRACE("construct sever");

    tcp protocol = ep.protocol();
    acceptor_.open(protocol);
    acceptor_.set_option(typename acceptor::reuse_address(true));
    acceptor_.bind(ep);
    acceptor_.listen(socket::max_listen_connections);
  }

  void start_accepting() noexcept {
    LOG_TRACE("start accepting");

    self self_ = this->shared_from_this();

    this->operator()(std::move(self_), error_code{}, socket{this->io_context_});
  }

  void stop_accepting() noexcept(false) {
    LOG_TRACE("stop accepting");

    acceptor_.cancel();
  }

  void close_all_sessions() {
    LOG_TRACE("close all sessions");

    for (session_ptr &session : sessions_) {
      if (session->is_open()) {
        session->close();
      }
    }

    sessions_.clear();
  }


private:
  void operator()(self self_, error_code err, socket sock) noexcept {
    if (err.failed()) {
      if (err.value() == asio::error::operation_aborted) {
        LOG_DEBUG("accepting canceled");
      } else {
        LOG_ERROR(err.message());
      }

      LOG_TRACE("break the server coroutine");
      return;
    }

    reenter(this) {
      for (;;) {
        yield acceptor_.async_accept(std::bind(&server_impl::operator(),
                                               this,
                                               std::move(self_),
                                               std::placeholders::_1,
                                               std::placeholders::_2));

        LOG_TRACE("accept new connection");
        try {
          LOG_DEBUG("accept connection from: %1%", sock.remote_endpoint());

          // at first remove already closed sessions
          sessions_.remove_if([](const session_ptr &session) {
            if (session->is_open() == false) {
              return true;
            }
            return false;
          });


          session::service_list services;
          for (const auto &[path, service_factory] : service_factories_) {
            services.emplace_back(http::url::path{path},
                                  service_factory->make_service());
          }


          session_ptr session =
              std::make_shared<restpp::session>(std::move(sock),
                                                services,
                                                req_headers_limit_,
                                                req_body_limit_,
                                                res_buffer_limit_);


          session->start();

          sessions_.emplace_back(std::move(session));

          LOG_DEBUG("sessions opened: %1%", sessions_.size());
        } catch (std::exception &e) {
          LOG_ERROR("catch expetion at session start: %1%", e.what());
          LOG_ERROR(e.what());
        }


        if (max_session_count_.has_value() &&
            sessions_.size() >= max_session_count_) {
          LOG_TRACE("reach sessions limit and stop accepting");

          acceptor_.cancel();

          yield wait_for_session_close(
              self_,
              std::make_shared<asio::steady_timer>(
                  this->io_context_,
                  std::chrono::seconds{WAIT_SESSION_CLOSE_SECONDS}));
        }
      }
    }
  }

  void wait_for_session_close(self                                self_,
                              std::shared_ptr<asio::steady_timer> timer) {
    // at first remove already closed sessions
    sessions_.remove_if([](const session_ptr &session) {
      if (session->is_open() == false) {
        return true;
      }
      return false;
    });

    if (sessions_.size() < max_session_count_) {
      LOG_TRACE("restart accepting");

      this->operator()(std::move(self_),
                       error_code{},
                       socket{this->io_context_});

      return;
    }

    timer->async_wait(std::bind(&server_impl::wait_for_session_close,
                                this,
                                std::move(self_),
                                std::move(timer)));
  }


private:
  asio::io_context &   io_context_;
  acceptor             acceptor_;
  std::string          root_path_;
  service_factory_list service_factories_;

  session_pool          sessions_;
  std::optional<size_t> max_session_count_;

  std::optional<size_t> req_headers_limit_;
  std::optional<size_t> req_body_limit_;
  std::optional<size_t> res_buffer_limit_;
};


server &server::async_run() {
  impl_->start_accepting();
  return *this;
}

server &server::stop() {
  impl_->stop_accepting();
  impl_->close_all_sessions();
  return *this;
}


server_builder::server_builder(asio::io_context &io_context)
    : io_context_{io_context} {
}

server_builder &server_builder::set_uri(std::string_view root) {
  root_ = root;
  return *this;
}

server_builder &server_builder::add_service(service_factory_ptr factory,
                                            std::string_view    path) {
  service_factories_.emplace_back(path, std::move(factory));
  return *this;
}

server_builder &server_builder::set_max_session_count(size_t count) {
  if (count != 0) {
    max_session_count_ = count;
  } else {
    max_session_count_.reset();
  }
  return *this;
}


server_builder &server_builder::set_req_headers_limit(size_t limit) {
  req_headers_limit_ = limit;
  return *this;
}

server_builder &server_builder::set_req_body_limit(size_t limit) {
  req_body_limit_ = limit;
  return *this;
}

server_builder &server_builder::set_res_buffer_limit(size_t limit) {
  res_buffer_limit_ = limit;
  return *this;
}


server server_builder::build() const {
  const std::regex uri_regex{R"(^http://([a-zA-Z0-9\.]+):(\d+)(.*))"};

  std::smatch uri_match;
  if (std::regex_match(root_, uri_match, uri_regex) == false ||
      uri_match.size() < 3) {
    LOG_THROW(std::invalid_argument,
              "invalid uri, must match: http://addr:port/not/required/path");
  }


  std::string addr = uri_match[1];
  std::string port = uri_match[2];
  std::string root = "";
  if (uri_match.size() >= 4) {
    root = uri_match[3];
  }


  tcp::resolver resolver{io_context_};
  tcp::endpoint endpoint = resolver.resolve(addr, port)->endpoint();


  LOG_DEBUG("listen endpoint: %1%", endpoint);


  service_factory_list service_factories;
  for (const auto &[relative, factory] : service_factories_) {
    http::url::path root_path{root};
    http::url::path relative_path{relative};

    service_factories.emplace_back(root_path / relative_path, factory);
  }


  std::shared_ptr<server_impl> impl =
      std::make_shared<server_impl>(io_context_,
                                    endpoint,
                                    service_factories,
                                    max_session_count_,
                                    req_headers_limit_,
                                    req_body_limit_,
                                    res_buffer_limit_);


  server retval{};
  retval.impl_ = impl;

  return retval;
}
} // namespace restpp
