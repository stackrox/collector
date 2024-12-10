package suites

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/assert"
	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

var (
	normalizedIp = "255.255.255.255"
	externalIp   = "8.8.8.8"
	serverPort   = 53
	externalUrl  = fmt.Sprintf("http://%s:%d", externalIp, serverPort)

	activeNormalizedConnection = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NilTimestamp,
	}

	activeUnnormalizedConnection = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", externalIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NilTimestamp,
	}

	inactiveNormalizedConnection = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: "Not nill time",
	}

	inactiveUnnormalizedConnection = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", externalIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: "Not nill time",
	}

	runtimeConfigDir  = "/tmp/collector-test"
	runtimeConfigFile = filepath.Join(runtimeConfigDir, "/runtime_config.yaml")
	collectorIP       = "localhost"
)

type RuntimeConfigFileTestSuite struct {
	IntegrationTestSuiteBase
	ClientContainer string
}

func (s *RuntimeConfigFileTestSuite) setRuntimeConfig(runtimeConfigFile string, configStr string) {
	err := os.WriteFile(runtimeConfigFile, []byte(configStr), 0666)
	s.Require().NoError(err)
}

func (s *RuntimeConfigFileTestSuite) getRuntimeConfigEnabledStr(enabled bool) string {
	var runtimeConfig types.RuntimeConfig
	runtimeConfig.Networking.ExternalIps.Enable = enabled

	configStr, err := runtimeConfig.GetRuntimeConfigStr()
	s.Require().NoError(err)

	return configStr
}

func (s *RuntimeConfigFileTestSuite) setExternalIpsEnabled(runtimeConfigFile string, enabled bool) {
	runtimeConfigStr := s.getRuntimeConfigEnabledStr(enabled)
	s.setRuntimeConfig(runtimeConfigFile, runtimeConfigStr)
}

// Launches collector and creates the directory for runtime configuration.
func (s *RuntimeConfigFileTestSuite) SetupTest() {
	s.RegisterCleanup("external-connection")

	s.StartContainerStats()

	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    "external-connection",
			Image:   config.Images().QaImageByKey("qa-alpine-curl"),
			Command: []string{"sh", "-c", "while true; do curl " + externalUrl + "; sleep 1; done"},
		})
	s.Require().NoError(err)
	s.ClientContainer = common.ContainerShortID(containerID)

	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_AFTERGLOW_PERIOD":               "6",
			"ROX_COLLECTOR_INTROSPECTION_ENABLE": "true",
		},
	}

	s.createDirectory(runtimeConfigDir)
	s.StartCollector(false, &collectorOptions)
}

func (s *RuntimeConfigFileTestSuite) AfterTest(suiteName, testName string) {
	s.StopCollector()
	s.cleanupContainers("external-connection")
	s.WritePerfResults()
	s.deleteFile(runtimeConfigFile)
}

func (s *RuntimeConfigFileTestSuite) TearDownSuite() {
	s.deleteDirectory(runtimeConfigDir)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFileEnable() {
	// The runtime config file was deleted before starting collector.
	// Default configuration is external IPs disabled.
	// We expect normalized connections.
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnection}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// External IPs enabled.
	// Normalized connection must be reported as inactive
	// Unnormalized connection will now be reported.
	s.setExternalIpsEnabled(runtimeConfigFile, true)
	assert.AssertExternalIps(s.T(), true, collectorIP)
	expectedConnections = append(expectedConnections, activeUnnormalizedConnection, inactiveNormalizedConnection)
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// The runtime config file is deleted. This disables external IPs. The normalized connection should be active
	// and the unnormalized connection shoul be inactive.
	s.deleteFile(runtimeConfigFile)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections = append(expectedConnections, activeNormalizedConnection, inactiveUnnormalizedConnection)
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// Back to having external IPs enabled.
	s.setExternalIpsEnabled(runtimeConfigFile, true)
	assert.AssertExternalIps(s.T(), true, collectorIP)
	expectedConnections = append(expectedConnections, activeUnnormalizedConnection, inactiveNormalizedConnection)
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFileDisable() {
	// The runtime config file was deleted before starting collector.
	// Default configuration is external IPs disabled.
	// We expect normalized connections.
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnection}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// The runtime config file is created, but external IPs is disables.
	// There is no change in the state, so there are no changes to the connections
	s.setExternalIpsEnabled(runtimeConfigFile, false)
	assert.AssertExternalIps(s.T(), false, collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// Back to using default behavior of external IPs disabled with no file.
	s.deleteFile(runtimeConfigFile)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFileInvalid() {
	// The runtime config file was deleted before starting collector.
	// Default configuration is external IPs disabled.
	// We expect normalized connections.
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnection}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// Testing an invalid configuration. There should not be a change in the configuration or reported connections
	invalidConfig := "asdf"
	s.setRuntimeConfig(runtimeConfigFile, invalidConfig)
	assert.AssertExternalIps(s.T(), false, collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}
