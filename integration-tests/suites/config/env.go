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
)

func ReadEnvVar(env string) string {
	if e, ok := os.LookupEnv(env); ok {
		return e
	}
	return ""
}

func ReadEnvVarWithDefault(env string, def string) string {
	if e, ok := os.LookupEnv(env); ok {
		return e
	}
	return def
}

func ReadBoolEnvVar(env string) bool {
	e, err := strconv.ParseBool(ReadEnvVarWithDefault(env, "false"))
	if err != nil {
		return false
	}
	return e
}
