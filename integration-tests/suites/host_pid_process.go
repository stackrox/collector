package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

// HostPidProcessTestSuite verifies that collector detects processes in
// containers running with the host PID namespace (--pid=host / hostPID: true).
//
// When a container shares the host PID namespace, pid == vpid for all its
// processes. The sinsp filter must fall back to cgroup-based container
// detection to avoid silently dropping these events.
type HostPidProcessTestSuite struct {
	IntegrationTestSuiteBase
	containerID string
}

func (s *HostPidProcessTestSuite) SetupSuite() {
	s.RegisterCleanup("host-pid-nginx")
	s.StartCollector(false, nil)

	imageStore := config.Images()
	image := imageStore.QaImageByKey("qa-nginx")

	err := s.Executor().PullImage(image)
	s.Require().NoError(err)

	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    "host-pid-nginx",
			Image:   image,
			PidMode: "host",
			Ports:   []uint16{80},
		})
	s.Require().NoError(err)
	s.containerID = common.ContainerShortID(containerID)

	// Run a command inside the container so we have a known process
	// to look for beyond the nginx entrypoint.
	_, err = s.execContainer("host-pid-nginx", []string{"sleep", "5"}, false)
	s.Require().NoError(err)
}

func (s *HostPidProcessTestSuite) TearDownSuite() {
	s.WritePerfResults()
}

// TestHostPidProcesses verifies that processes inside a --pid=host container
// are reported to Sensor despite sharing the host PID namespace.
func (s *HostPidProcessTestSuite) TestHostPidProcesses() {
	expectedProcesses := []types.ProcessInfo{
		{
			Name:    "nginx",
			ExePath: "/usr/sbin/nginx",
			Uid:     0,
			Gid:     0,
			Args:    "-g daemon off;",
		},
		{
			Name:    "sleep",
			ExePath: "/bin/sleep",
			Uid:     0,
			Gid:     0,
			Args:    "5",
		},
	}

	s.Sensor().ExpectProcesses(s.T(), s.containerID, 30*time.Second, expectedProcesses...)
}

// TestNoHostProcesses verifies that host processes (those not in any
// container) are filtered out and not reported to Sensor. Processes
// with an empty container ID are host processes.
func (s *HostPidProcessTestSuite) TestNoHostProcesses() {
	// Give collector some time to process events, then check that no
	// host processes (empty container ID) have been reported.
	time.Sleep(10 * time.Second)

	hostProcesses := s.Sensor().Processes("")
	s.Assert().Empty(hostProcesses,
		"Expected no host processes (empty container ID) to be reported, but found %d", len(hostProcesses))
}
