package config

import (
	"os"
	"strconv"
)

const (
	envCollectionMethod = "COLLECTION_METHOD"
	envCollectorImage   = "COLLECTOR_IMAGE"

	envCollectorLogLevel     = "COLLECTOR_LOG_LEVEL"
	envCollectorOfflineMode  = "COLLECTOR_OFFLINE_MODE"
	envCollectorPreArguments = "COLLECTOR_PRE_ARGUMENTS"

	envHostType    = "REMOTE_HOST_TYPE"
	envHostUser    = "REMOTE_HOST_USER"
	envHostAddress = "REMOTE_HOST_ADDRESS"
	envHostOptions = "REMOTE_HOST_OPTIONS"

	envVMInstanceType = "VM_INSTANCE_TYPE"
	envVMConfig       = "VM_CONFIG"

	envRuntimeCommand = "RUNTIME_COMMAND"
	envRuntimeSocket  = "RUNTIME_SOCKET"
	envRuntimeAsRoot  = "RUNTIME_AS_ROOT"

	envQATag = "COLLECTOR_QA_TAG"

	envPerfCommand     = "COLLECTOR_PERF_COMMAND"
	envBpftraceCommand = "COLLECTOR_BPFTRACE_COMMAND"
	envBccCommand      = "COLLECTOR_BCC_COMMAND"
	envSkipHeadersInit = "COLLECTOR_SKIP_HEADERS_INIT"

	envStopTimeout = "STOP_TIMEOUT"
)

// ReadEnvVar safely reads a variable from the environment.
// If the variable does not exist, an empty string is returned.
func ReadEnvVar(env string) string {
	if e, ok := os.LookupEnv(env); ok {
		return e
	}
	return ""
}

// ReadEnvVarWithDefault safely reads a variable from the environment.
// If the variable does not exist, the provided default is returned.
func ReadEnvVarWithDefault(env string, def string) string {
	if e, ok := os.LookupEnv(env); ok {
		return e
	}
	return def
}

// ReadBoolEnvVar safely reads a boolean value from the environment,
// parsed into a bool type. If the variable does not exist, the result is false
func ReadBoolEnvVar(env string) bool {
	e, err := strconv.ParseBool(ReadEnvVarWithDefault(env, "false"))
	if err != nil {
		return false
	}
	return e
}
