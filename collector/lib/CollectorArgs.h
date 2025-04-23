#pragma once

#include <optional>
#include <string>

#include <json/json.h>

#include "CollectionMethod.h"
#include "optionparser.h"

namespace collector {

class CollectorArgs {
 public:
  static CollectorArgs* getInstance();

  bool parse(int argc, char** argv, int& exitCode);
  void clear();

  option::ArgStatus checkCollectorConfig(const option::Option& option, bool msg);
  option::ArgStatus checkCollectionMethod(const option::Option& option, bool msg);
  option::ArgStatus checkGRPCServer(const option::Option& option, bool msg);
  option::ArgStatus checkOptionalNumeric(const option::Option& option, bool msg);

  const Json::Value& CollectorConfig() const;
  std::optional<CollectionMethod> GetCollectionMethod() const;
  const std::string& GRPCServer() const;
  const std::string& Message() const;

 private:
  CollectorArgs();
  ~CollectorArgs();

  static CollectorArgs* instance;

  Json::Value collectorConfig;
  std::optional<CollectionMethod> collectionMethod{std::nullopt};
  std::string message;
  std::string grpcServer;
};

} /* namespace collector */
