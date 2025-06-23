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

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	"github.com/stackrox/rox/generated/storage"
	"github.com/stackrox/rox/pkg/protoconv"
)

var (
	normalizedIp = "255.255.255.255"
	externalIp   = "8.8.8.8"
	serverPort   = uint32(53)
	externalUrl  = fmt.Sprintf("http://%s:%d", externalIp, serverPort)
	notNilTime   = protoconv.ConvertTimeToTimestamp(time.Now())

	activeNormalizedConnection = sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", 0),
		RemoteAddress:  types.CreateNetworkAddress(normalizedIp, "", serverPort),
		Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
		Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: nil,
	}

	activeUnnormalizedConnection = sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", 0),
		RemoteAddress:  types.CreateNetworkAddress("", externalIp, serverPort),
		Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
		Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: nil,
	}

	inactiveNormalizedConnection = sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", 0),
		RemoteAddress:  types.CreateNetworkAddress(normalizedIp, "", serverPort),
		Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
		Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: notNilTime,
	}

	inactiveUnnormalizedConnection = sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", 0),
		RemoteAddress:  types.CreateNetworkAddress("", externalIp, serverPort),
		Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
		Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: notNilTime,
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

func (s *RuntimeConfigFileTestSuite) setExternalIpsEnabled(runtimeConfigFile string, enabled string) {
	var runtimeConfig types.RuntimeConfig
	runtimeConfig.Networking.ExternalIps.Enabled = enabled
	runtimeConfigStr, err := runtimeConfig.GetRuntimeConfigStr()
	s.Require().NoError(err)
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
	expectedConnections := []types.NetworkConnectionBatch{[]*sensorAPI.NetworkConnection{&activeNormalizedConnection}}
	connectionSuccess := s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.ClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)

	// External IPs enabled.
	// Normalized connection must be reported as inactive
	// Unnormalized connection will now be reported.
	s.setExternalIpsEnabled(runtimeConfigFile, "ENABLED")
	assert.AssertExternalIps(s.T(), "ENABLED", collectorIP)
	expectedConnections = append(expectedConnections, []*sensorAPI.NetworkConnection{&activeUnnormalizedConnection, &inactiveNormalizedConnection})
	connectionSuccess = s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.ClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)

	// The runtime config file is deleted. This disables external IPs. The normalized connection should be active
	// and the unnormalized connection shoul be inactive.
	s.deleteFile(runtimeConfigFile)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections = append(expectedConnections, []*sensorAPI.NetworkConnection{&activeNormalizedConnection, &inactiveUnnormalizedConnection})
	connectionSuccess = s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.ClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)

	// Back to having external IPs enabled.
	s.setExternalIpsEnabled(runtimeConfigFile, "ENABLED")
	assert.AssertExternalIps(s.T(), "ENABLED", collectorIP)
	expectedConnections = append(expectedConnections, []*sensorAPI.NetworkConnection{&activeUnnormalizedConnection, &inactiveNormalizedConnection})
	connectionSuccess = s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.ClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFileDisable() {
	// The runtime config file was deleted before starting collector.
	// Default configuration is external IPs disabled.
	// We expect normalized connections.
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []*sensorAPI.NetworkConnection{&activeNormalizedConnection}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// The runtime config file is created, but external IPs is disables.
	// There is no change in the state, so there are no changes to the connections
	s.setExternalIpsEnabled(runtimeConfigFile, "DISABLED")
	assert.AssertExternalIps(s.T(), "DISABLED", collectorIP)
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
	expectedConnections := []*sensorAPI.NetworkConnection{&activeNormalizedConnection}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// Testing an invalid configuration. There should not be a change in the configuration or reported connections
	invalidConfig := "asdf"
	s.setRuntimeConfig(runtimeConfigFile, invalidConfig)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.ClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}
