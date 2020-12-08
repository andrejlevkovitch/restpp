// service.hpp

#pragma once

#include <memory>
#include <restpp/http/request.hpp>
#include <restpp/http/response.hpp>
#include <restpp/misc.hpp>


namespace restpp {
/**\brief service handles requests for some path
 * \note that service should be lightweight and fast constuctible, because it
 * creates for every session
 */
class service {
public:
  virtual ~service() = default;

  /**\brief calls before sesstion start
   * \param remote remote address
   * \note if method throw exception, then session doesn't starts
   */
  virtual void at_session_start([[maybe_unused]] std::string_view remote){};

  /**\brief calls after session close
   */
  virtual void at_session_close() noexcept {};


  virtual void handle(http::request req, OUTPUT http::response &res) = 0;
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
} // namespace restpp
