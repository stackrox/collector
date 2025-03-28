#include "CivetWrapper.h"

#include <sstream>

namespace collector {

QueryParams CivetWrapper::ParseParameters(const char* queryString) {
  QueryParams params;

  if (queryString == nullptr) {
    return params;
  }

  std::stringstream query_stringstream(queryString);
  while (query_stringstream.good()) {
    std::string statement;

    std::getline(query_stringstream, statement, '&');

    size_t equal = statement.find('=');

    if (equal != std::string::npos) {
      params[statement.substr(0, equal)] = statement.substr(equal + 1);
    }
  }
  return params;
}

std::optional<std::string> CivetWrapper::GetParameter(const QueryParams& params, const std::string& paramName) {
  return params.count(paramName) != 0 ? std::make_optional(params.at(paramName)) : std::nullopt;
}

}  // namespace collector
