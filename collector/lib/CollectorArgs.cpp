#include "CollectorArgs.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "grpc/GRPC.h"

#include "CollectorConfig.h"
#include "log/Logging.h"
#include "optionparser.h"

namespace collector {

enum optionIndex {
  UNKNOWN,
  HELP,
  COLLECTOR_CONFIG,
  COLLECTION_METHOD,
  GRPC_SERVER,
};

static option::ArgStatus
checkCollectorConfig(const option::Option& option, bool msg) {
  return CollectorArgs::getInstance()->checkCollectorConfig(option, msg);
}

static option::ArgStatus
checkCollectionMethod(const option::Option& option, bool msg) {
  return CollectorArgs::getInstance()->checkCollectionMethod(option, msg);
}

static option::ArgStatus
checkGRPCServer(const option::Option& option, bool msg) {
  return CollectorArgs::getInstance()->checkGRPCServer(option, msg);
}

static const option::Descriptor usage[] =
    {
        {UNKNOWN, 0, "", "", option::Arg::None,
         "USAGE: collector [options]\n\n"
         "Options:"},
        {HELP, 0, "", "help", option::Arg::None, "  --help                \tPrint usage and exit."},
        {COLLECTOR_CONFIG, 0, "", "collector-config", checkCollectorConfig, "  --collector-config    \tCollector config as a JSON string. Please refer to documentation on the valid JSON format."},
        {COLLECTION_METHOD, 0, "", "collection-method", checkCollectionMethod, "  --collection-method   \tCollection method (ebpf or core_bpf)."},
        {GRPC_SERVER, 0, "", "grpc-server", checkGRPCServer, "  --grpc-server         \tGRPC server endpoint string in the form HOST1:PORT1."},
        {UNKNOWN, 0, "", "", option::Arg::None,
         "\nExamples:\n"
         "  collector --grpc-server=\"172.16.0.5:443\"\n"},
        {0, 0, 0, 0, 0, 0},
};

CollectorArgs::CollectorArgs() {
}

CollectorArgs::~CollectorArgs() {
}

CollectorArgs* CollectorArgs::instance;

CollectorArgs*
CollectorArgs::getInstance() {
  if (instance == NULL) {
    instance = new CollectorArgs();
  }
  return instance;
}

void CollectorArgs::clear() {
  delete instance;
  instance = new CollectorArgs();
}

bool CollectorArgs::parse(int argc, char** argv, int& exitCode) {
  using std::stringstream;

  // Skip program name argv[0] if present
  argc -= (argc > 0);
  argv += (argc > 0);

  option::Stats stats(usage, argc, argv);
  option::Option options[stats.options_max], buffer[stats.buffer_max];
  option::Parser parse(usage, argc, argv, options, buffer);

  if (parse.error()) {
    exitCode = 1;
    return false;
  }

  if (options[HELP]) {
    stringstream out;
    option::printUsage(out, usage);
    message = out.str();
    exitCode = 0;
    return false;
  }

  for (int i = 0; i < parse.optionsCount(); ++i) {
    option::Option& opt = buffer[i];
    if (opt.index() == UNKNOWN) {
      stringstream out;

      out << "Unknown option: " << options[UNKNOWN].name;
      message = out.str();
      exitCode = 1;
      return false;
    }
  }

  exitCode = 0;
  return true;
}

option::ArgStatus
CollectorArgs::checkCollectionMethod(const option::Option& option, bool msg) {
  using namespace option;
  using std::string;

  if (option.arg == NULL) {
    if (msg) {
      this->message = "Missing collection method, using default.";
    }
    return ARG_OK;
  }

  collectionMethod = ParseCollectionMethod(option.arg);

  CLOG(DEBUG) << "CollectionMethod: " << CollectionMethodName(collectionMethod.value());

  return ARG_OK;
}

option::ArgStatus
CollectorArgs::checkCollectorConfig(const option::Option& option, bool msg) {
  using namespace option;
  Json::Value root;

  const auto& [status, message] = CollectorConfig::CheckConfiguration(option.arg, &root);
  if (msg) {
    this->message = message;
  }

  if (status == ARG_OK) {
    collectorConfig = root;
    CLOG(DEBUG) << "Collector config: " << collectorConfig.toStyledString();
  }

  return status;
}

option::ArgStatus
CollectorArgs::checkGRPCServer(const option::Option& option, bool msg) {
  using namespace option;
  const auto& [status, m] = CheckGrpcServer(option.arg);

  if (status == ARG_OK) {
    grpcServer = option.arg;
  }

  if (msg) {
    this->message = m;
  }

  return status;
}

const Json::Value&
CollectorArgs::CollectorConfig() const {
  return collectorConfig;
}

std::optional<CollectionMethod>
CollectorArgs::GetCollectionMethod() const {
  return collectionMethod;
}

const std::string&
CollectorArgs::GRPCServer() const {
  return grpcServer;
}

const std::string&
CollectorArgs::Message() const {
  return message;
}

} /* namespace collector */
