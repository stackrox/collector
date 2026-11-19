package suites

import (
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

// The trusted exepath feature (ROX-22498) resolves symlinks in execution
// paths. When ROX_COLLECTOR_ENABLE_TRUSTED_EXEPATH is enabled, the exe
// path reported for a process started through a symlink is the resolved
// path of the binary that actually ran; when disabled, it is the path as
// it was invoked.
//
// The suite starts two containers that run the sleep binary. The
// "symlinked" container execs it through the /tmp/sleep-link symlink,
// while the "plain" container execs it directly as a control that the
// flag does not affect non-symlinked executions.
type TrustedExePathTestSuite struct {
	IntegrationTestSuiteBase
	// Resolved indicates whether the collector runs with
	// ROX_COLLECTOR_ENABLE_TRUSTED_EXEPATH enabled, i.e. whether exe
	// paths are expected to be symlink-resolved.
	Resolved         bool
	symlinkedSleeper string
	plainSleeper     string
}

func (s *TrustedExePathTestSuite) SetupSuite() {
	s.RegisterCleanup("trusted-exepath-symlinked")
	s.RegisterCleanup("trusted-exepath-plain")

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			"turnOffScrape": false,
		},
		Env: map[string]string{
			"ROX_COLLECTOR_ENABLE_TRUSTED_EXEPATH": strconv.FormatBool(s.Resolved),
		},
	}

	s.StartCollector(false, &collectorOptions)

	processImage := config.Images().QaImageByKey("qa-thread-exec")

	symlinkedID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  "trusted-exepath-symlinked",
			Image: processImage,
			Entrypoint: []string{"/bin/sh", "-c",
				`ln -s /usr/bin/sleep /tmp/sleep-link && ` +
					`exec /tmp/sleep-link 300`},
		})
	s.Require().NoError(err)
	s.symlinkedSleeper = common.ContainerShortID(symlinkedID)

	plainID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  "trusted-exepath-plain",
			Image: processImage,
			Entrypoint: []string{"/bin/sh", "-c",
				`exec /usr/bin/sleep 300`},
		})
	s.Require().NoError(err)
	s.plainSleeper = common.ContainerShortID(plainID)
}

func (s *TrustedExePathTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("trusted-exepath-symlinked", "trusted-exepath-plain")
	s.WritePerfResults()
}

// TestSymlinkedExePath checks the exe path of a process started through
// a symlink: the symlink path when resolution is disabled, the resolved
// path of the binary when it is enabled.
func (s *TrustedExePathTestSuite) TestSymlinkedExePath() {
	expectedExePath := "/tmp/sleep-link"
	if s.Resolved {
		expectedExePath = "/usr/bin/sleep"
	}
	s.expectSleeper(s.symlinkedSleeper, "sleep-link", expectedExePath)
}

// TestDirectExePath checks that the exe path of a process started
// without a symlink is the same regardless of the flag.
func (s *TrustedExePathTestSuite) TestDirectExePath() {
	s.expectSleeper(s.plainSleeper, "sleep", "/usr/bin/sleep")
}

func (s *TrustedExePathTestSuite) expectSleeper(containerID string, name string, expectedExePath string) {
	s.Sensor().ExpectProcesses(s.T(), containerID, 10*time.Second,
		types.ProcessInfo{
			Name:    name,
			ExePath: expectedExePath,
			Uid:     0,
			Gid:     0,
			Args:    "300",
		})
}
