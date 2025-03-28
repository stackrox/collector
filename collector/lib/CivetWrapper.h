#ifndef CIVET_WRAPPER_H
#define CIVET_WRAPPER_H

#include <CivetServer.h>
#include <optional>
#include <unordered_map>

namespace collector {
using QueryParams = std::unordered_map<std::string, std::string>;

class CivetWrapper : public CivetHandler {
 public:
  virtual const std::string& GetBaseRoute() = 0;

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

#endif
