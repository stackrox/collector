#ifndef _COLLECTOR_ARGS_H_
#define _COLLECTOR_ARGS_H_

#include <string>

#include <json/json.h>

#include "optionparser.h"

namespace collector {

class CollectorArgs {
 public:
  static CollectorArgs* getInstance();

  bool parse(int argc, char** argv, int& exitCode);
  void clear();

  option::ArgStatus checkCollectorConfig(const option::Option& option, bool msg);
  option::ArgStatus checkCollectionMethod(const option::Option& option, bool msg);
  option::ArgStatus checkChisel(const option::Option& option, bool msg);
  option::ArgStatus checkGRPCServer(const option::Option& option, bool msg);
  option::ArgStatus checkOptionalNumeric(const option::Option& option, bool msg);

  const Json::Value& CollectorConfig() const;
  const std::string& CollectionMethod() const;
  const std::string& Chisel() const;
  const std::string& GRPCServer() const;
  const std::string& Message() const;

 private:
  CollectorArgs();
  ~CollectorArgs();

  static CollectorArgs* instance;

  Json::Value collectorConfig;
  std::string collectionMethod;
  std::string chisel;
  std::string message;
  std::string grpcServer;
};

} /* namespace collector */

#endif /* _COLLECTOR_ARGS_H_ */
