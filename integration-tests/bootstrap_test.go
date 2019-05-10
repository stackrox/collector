package integrationtests

import (
	"io/ioutil"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
)

// TestCollectorBootstrap only run locally
func TestCollectorBootstrap(t *testing.T) {
	if ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") == "local" {
		suite.Run(t, new(BootstrapTestSuite))
	}
}

type BootstrapTestSuite struct {
	suite.Suite
	executor Executor
}

func (s *BootstrapTestSuite) SetupSuite() {
	s.executor = NewExecutor()
}

func (s *BootstrapTestSuite) TestBootstrapScript() {
	tests := map[string]struct {
		env               map[string]string
		mounts            map[string]string
		osRelease         string
		expectedLogLines  []string
		expectedExitError bool
	}{
		"invalid collection method": {
			expectedExitError: true,
			env:               map[string]string{"COLLECTION_METHOD": "telepathy"},
			expectedLogLines:  []string{"Error: Collector configured with invalid value: COLLECTION_METHOD=telepathy"},
		},
		"module on cos switch": {
			expectedExitError: true,
			env: map[string]string{
				"COLLECTION_METHOD": "kernel-module",
				"KERNEL_VERSION":    "4.14.104+",
			},
			osRelease: "ID=cos\nBUILD_ID=11895.86.0\nPRETTY_NAME=\"Container-Optimized OS from Google\"\n",
			expectedLogLines: []string{
				"Error: \"Container-Optimized OS from Google\" does not support third-party kernel modules",
				"Warning: Switching to eBPF based collection, please configure RUNTIME_SUPPORT=ebpf",
			},
		},
		"module on cos fail": {
			expectedExitError: true,
			env: map[string]string{
				"COLLECTION_METHOD": "kernel-module",
				"KERNEL_VERSION":    "4.9.104+",
			},
			osRelease: "ID=cos\nBUILD_ID=11895.86.0\nPRETTY_NAME=\"Container-Optimized OS from Google\"\n",
			expectedLogLines: []string{
				"Error: \"Container-Optimized OS from Google\" does not support third-party kernel modules",
				"This program will now exit and retry when it is next restarted.",
			},
		},
		"backwards compatability success": {
			env: map[string]string{
				"COLLECTION_METHOD": "",
				"KERNEL_VERSION":    "4.10.0-1004-gcp",
				"COLLECTOR_CONFIG":  `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
			},
			mounts: map[string]string{
				"/host/etc:ro":     "/tmp",
				"/host/usr/lib:ro": "/tmp",
			},
			expectedLogLines: []string{
				"Starting StackRox Collector...",
			},
		},
		"backwards compatability ebpf switch": {
			env: map[string]string{
				"COLLECTION_METHOD": "",
				"KERNEL_VERSION":    "4.10.0-1004-gcp",
				"COLLECTOR_CONFIG":  `{"useEbpf":true,"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
			},
			mounts: map[string]string{
				"/host/etc:ro":     "/tmp",
				"/host/usr/lib:ro": "/tmp",
			},
			expectedLogLines: []string{
				"Error: Linux 4.10.0-1004-gcp does not support ebpf based collection.",
				"Warning: Switching to kernel module based collection, please configure RUNTIME_SUPPORT=kernel-module",
				"Starting StackRox Collector...",
			},
		},
	}

	for name, tc := range tests {
		s.T().Run(name, func(t *testing.T) {
			collector := NewCollectorManager(s.executor)
			collector.DisableGrpcServer = true
			collector.BootstrapOnly = true

			err := collector.Setup()
			require.NoError(t, err)

			for k, v := range tc.env {
				collector.Env[k] = v
			}

			for k, v := range tc.mounts {
				collector.Mounts[k] = v
			}

			// Create mock os-release file
			if len(tc.osRelease) > 0 {
				tmp, err := ioutil.TempFile("/tmp/", "os-release-")
				require.NoError(t, err)
				defer os.Remove(tmp.Name())
				_, err = tmp.WriteString(tc.osRelease)
				require.NoError(t, err)
				err = tmp.Close()
				require.NoError(t, err)
				collector.Mounts["/host/etc/os-release:ro"] = tmp.Name()
				delete(collector.Mounts, "/etc/")
			}

			err = collector.Launch()
			if !tc.expectedExitError {
				require.NoError(t, err)
			}
			for _, logLine := range tc.expectedLogLines {
				assert.Contains(t, collector.CollectorOutput, logLine)
			}

			err = collector.TearDown()
			require.NoError(t, err)
		})
	}
}
