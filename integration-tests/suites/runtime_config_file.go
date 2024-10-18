package suites

import (
	"path/filepath"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/introspection_endpoints"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

type RuntimeConfigFileTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *RuntimeConfigFileTestSuite) createDir(runtimeConfigDir string) {
	_, err := s.execContainer("collector", []string{"mkdir", runtimeConfigDir})
	s.Require().NoError(err)
}

func (s *RuntimeConfigFileTestSuite) deleteFile(file string) {
	_, err := s.execContainer("collector", []string{"rm", file})
	s.Require().NoError(err)
}

func (s *RuntimeConfigFileTestSuite) setExternalIpsEnable(runtimeConfigFile string, enable bool) {
	configStr := "networking:\n  externalIps:\n    enable: " + strconv.FormatBool(enable)
	_, err := s.execContainer("collector", []string{"/bin/bash", "-c", "echo '" + configStr + "' > " + runtimeConfigFile})
	s.Require().NoError(err)
}

// Launches collector and creates the directory for runtime configuration.
// Writes various things to the file for runtime configuration and checks
// the config introspection endpoint to make sure that the configuration is
// correct.
func (s *RuntimeConfigFileTestSuite) SetupSuite() {
	s.StartContainerStats()

	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_AFTERGLOW_PERIOD":               "0",
			"ROX_ENABLE_AFTERGLOW":               "false",
			"ROX_COLLECTOR_INTROSPECTION_ENABLE": "true",
		},
	}

	externalIpsTrue := types.RuntimeConfig{
		Networking: types.Networking{
			ExternalIps: types.ExternalIps{
				Enable: true,
			},
		},
	}

	externalIpsFalse := types.RuntimeConfig{
		Networking: types.Networking{
			ExternalIps: types.ExternalIps{
				Enable: false,
			},
		},
	}

	s.StartCollector(false, &collectorOptions)
	runtimeConfigDir := "/etc/stackrox"
	runtimeConfigFile := filepath.Join(runtimeConfigDir, "/runtime_config.yaml")
	s.createDir(runtimeConfigDir)

	runtimeConfigSuccess := introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, true)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsTrue)
	s.Require().True(runtimeConfigSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, false)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)

	s.deleteFile(runtimeConfigFile)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, true)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsTrue)
	s.Require().True(runtimeConfigSuccess)

	s.deleteFile(runtimeConfigFile)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 60*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, false)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
}

func (s *RuntimeConfigFileTestSuite) TearDownSuite() {
	s.StopCollector()
	s.WritePerfResults()
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFile() {
}
