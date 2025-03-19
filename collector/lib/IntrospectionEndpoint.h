#ifndef _INTROSPECTION_ENDPOINT_
#define _INTROSPECTION_ENDPOINT_

#include <optional>
#include <unordered_map>

#include "CivetWrapper.h"

namespace collector {

using QueryParams = std::unordered_map<std::string, std::string>;

class IntrospectionEndpoint : public CivetWrapper {
 protected:
  static QueryParams ParseParameters(const char* queryString);
  static std::optional<std::string> GetParameter(const QueryParams& params, const std::string& paramName);

  static bool ServerError(struct mg_connection* conn, const char* err) {
    return mg_send_http_error(conn, 500, err) >= 0;
  }
  static bool ClientError(struct mg_connection* conn, const char* err) {
    return mg_send_http_error(conn, 400, err) >= 0;
  }
};

}  // namespace collector

#endif  // _INTROSPECTION_ENDPOINT_
