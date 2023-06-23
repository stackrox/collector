package runtime

import (
	"github.com/stackrox/collector/integration-tests/suites/common"
)

type PodmanRuntime struct {
	DockerRuntime
}

func NewPodmanRuntime(executor common.Executor) Runtime {
	return &PodmanRuntime{
		DockerRuntime: DockerRuntime{
			command:    "podmand",
			executor:   executor,
			containers: make(map[string]*Container),
		},
	}
}
