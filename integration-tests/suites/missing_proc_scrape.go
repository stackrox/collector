package suites

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stretchr/testify/assert"
)

type MissingProcScrapeTestSuite struct {
	IntegrationTestSuiteBase
}

const (
	fakePid      = 8989
	fakeProcName = "short"
	fakeProcDir  = "/tmp/fake-proc"
)

func (s *MissingProcScrapeTestSuite) createFakeProcDir() {
	fakeProc := fmt.Sprintf("%s/%d", fakeProcDir, fakePid)
	err := os.MkdirAll(fakeProc, os.ModePerm)
	s.Require().NoError(err, "Failed to create fake proc directory")

	err = os.Symlink("/bin/sh", filepath.Join(fakeProc, "exe"))
	s.Require().NoError(err, "Failed to symlink exe")

	status := fmt.Sprintf("Name: %s\n", fakeProcName)
	err = ioutil.WriteFile(filepath.Join(fakeProc, "status"), []byte(status), 0644)
	s.Require().NoError(err, "Failed to create status file")

	cmdline := fmt.Sprintf("/bin/%s\n", fakeProcName)
	err = ioutil.WriteFile(filepath.Join(fakeProc, "cmdline"), []byte(cmdline), 0644)
	s.Require().NoError(err, "Failed to create cmdline file")

	f, err := os.OpenFile(filepath.Join(fakeProc, "environ"), os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	s.Require().NoError(err, "Failed to create environ file")
	defer f.Close()

	for _, v := range os.Environ() {
		_, err := f.WriteString(v + "\n")
		s.Require().NoError(err, "Failed to write to environ")
	}

	err = ioutil.WriteFile(filepath.Join(fakeProcDir, "stat"), []byte("btime 1695972922\n"), 0644)
	s.Require().NoError(err, "Failed to write to stat")

	err = os.MkdirAll(filepath.Join(fakeProcDir, "1"), os.ModePerm)
	s.Require().NoError(err, "Failed to create /proc/1")

	err = ioutil.WriteFile(filepath.Join(fakeProcDir, "1", "mounts"), []byte("cgroup2 /sys/fs/cgroup cgroup2 rw,seclabel,nosuid,nodev,noexec,relatime,nsdelegate,memory_recursiveprot 0 0\n"), 0644)
	s.Require().NoError(err, "Failed to write to /proc/1/mounts")
}

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	s.RegisterCleanup()

	_, err := os.Stat(fakeProcDir)
	if os.IsNotExist(err) {
		s.createFakeProcDir()
	}

	collectorOptions := collector.StartupOptions{
		Mounts: map[string]string{
			"/host/proc:ro": fakeProcDir,
		},
		Env: map[string]string{
			// if /etc/hostname is empty collector will read /proc/sys/kernel/hostname
			// which will also be empty because of the fake proc, so collector will exit.
			// to avoid this, set NODE_HOSTNAME
			"NODE_HOSTNAME": "collector-missing-proc-host",
		},
	}

	s.StartCollector(false, &collectorOptions)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	s.StopCollector()
}
