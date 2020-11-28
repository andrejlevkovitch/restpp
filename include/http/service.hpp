// service.hpp

#pragma once

#include "http/request.hpp"
#include "http/response.hpp"
#include "misc.hpp"
#include <memory>


namespace http {
/**\brief service handles requests for some path
 * \note that service should be lightweight and fast constuctible, because it
 * creates for every session
 */
class service {
public:
#define DEFAULT_HANDLER                                                        \
  { return this->handle(std::move(req), res); }

  virtual ~service() = default;

  /**\brief calls before sesstion start
   * \param remote remote address
   * \note if method throw exception, then session doesn't starts
   */
  virtual void at_session_start([[maybe_unused]] std::string_view remote){};

  /**\brief calls after session close
   */
  virtual void at_session_close() noexcept {};


  virtual void handle(request req, OUTPUT response &res) = 0;
  virtual void handleGET(request req, OUTPUT response &res) DEFAULT_HANDLER;
  virtual void handleHEAD(request req, OUTPUT response &res) DEFAULT_HANDLER;
  virtual void handlePUT(request req, OUTPUT response &res) DEFAULT_HANDLER;
  virtual void handlePOST(request req, OUTPUT response &res) DEFAULT_HANDLER;
  virtual void handlePATCH(request req, OUTPUT response &res) DEFAULT_HANDLER;
  virtual void handleDELETE(request req, OUTPUT response &res) DEFAULT_HANDLER;
};

using service_ptr = std::shared_ptr<service>;


/**\brief service instance creates for every session, so service is a
 * thread-safe object
 */
class service_factory {
public:
  virtual ~service_factory() = default;

  virtual service_ptr make_service() = 0;
};

using service_factory_ptr = std::shared_ptr<service_factory>;
} // namespace http
