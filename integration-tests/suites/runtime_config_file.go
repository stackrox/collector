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

	activeNormalizedConnectionEgress = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NilTimestamp,
	}

	activeUnnormalizedConnectionEgress = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", externalIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NilTimestamp,
	}

	inactiveNormalizedConnectionEgress = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: "Not nill time",
	}

	inactiveUnnormalizedConnectionEgress = types.NetworkInfo{
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
	EgressClientContainer  string
	IngressClientContainer string
	IngressServerContainer string
}

func (s *RuntimeConfigFileTestSuite) writeRuntimeConfig(runtimeConfigFile string, configStr string) {
	err := os.WriteFile(runtimeConfigFile, []byte(configStr), 0666)
	s.Require().NoError(err)
}

func (s *RuntimeConfigFileTestSuite) setRuntimeConfig(config types.RuntimeConfig) {
	s.writeRuntimeConfig(runtimeConfigFile, config.String())
}

func (s *RuntimeConfigFileTestSuite) runBerserkerContainers() (client, server string) {
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:        "external-connection-ingress-client",
			Image:       config.Images().QaImageByKey("performance-berserker"),
			Privileged:  true,
			NetworkMode: "host",
			Entrypoint: []string{
				"/scripts/init.sh",
			},
			Env: map[string]string{
				"RUST_LOG":  "DEBUG",
				"IS_CLIENT": "true",
			},
		},
	)
	s.Require().NoError(err)
	client = common.ContainerShortID(containerID)

	containerID, err = s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:        "external-connection-ingress-server",
			Image:       config.Images().QaImageByKey("performance-berserker"),
			Privileged:  true,
			NetworkMode: "host",
			Entrypoint: []string{
				"/scripts/init.sh",
			},
			Env: map[string]string{
				"RUST_LOG":  "DEBUG",
				"IS_CLIENT": "false",
			},
		},
	)
	s.Require().NoError(err)
	server = common.ContainerShortID(containerID)

	return client, server
}

func (s *RuntimeConfigFileTestSuite) teardownBerserkerContainers() {
	s.cleanupContainers("external-connection-ingress-server", "external-connection-ingress-client")
}

// Launches collector and creates the directory for runtime configuration.
func (s *RuntimeConfigFileTestSuite) SetupTest() {
	s.RegisterCleanup("external-connection")

	s.StartContainerStats()

	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    "external-connection-egress",
			Image:   config.Images().QaImageByKey("qa-alpine-curl"),
			Command: []string{"sh", "-c", "while true; do curl " + externalUrl + "; sleep 1; done"},
		})
	s.Require().NoError(err)
	s.EgressClientContainer = common.ContainerShortID(containerID)

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
	s.cleanupContainers(
		"external-connection-egress",
		"external-connection-ingress-client",
		"external-connection-ingress-server",
	)
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
	expectedConnections := []types.NetworkInfoBatch{[]types.NetworkInfo{activeNormalizedConnectionEgress}}
	connectionSuccess := s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)

	// External IPs enabled.
	// Normalized connection must be reported as inactive
	// Unnormalized connection will now be reported.
	s.setRuntimeConfig(types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled: "ENABLED",
			},
		},
	})
	assert.AssertExternalIps(s.T(), "ENABLED", collectorIP)
	expectedConnections = append(expectedConnections, []types.NetworkInfo{activeUnnormalizedConnectionEgress, inactiveNormalizedConnectionEgress})
	connectionSuccess = s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)

	// The runtime config file is deleted. This disables external IPs. The normalized connection should be active
	// and the unnormalized connection shoul be inactive.
	s.deleteFile(runtimeConfigFile)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections = append(expectedConnections, []types.NetworkInfo{activeNormalizedConnectionEgress, inactiveUnnormalizedConnectionEgress})
	connectionSuccess = s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)

	// Back to having external IPs enabled.
	s.setRuntimeConfig(types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled: "ENABLED",
			},
		},
	})
	assert.AssertExternalIps(s.T(), "ENABLED", collectorIP)
	expectedConnections = append(expectedConnections, []types.NetworkInfo{activeUnnormalizedConnectionEgress, inactiveNormalizedConnectionEgress})
	connectionSuccess = s.Sensor().ExpectSameElementsConnectionsScrapes(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFileDisable() {
	// The runtime config file was deleted before starting collector.
	// Default configuration is external IPs disabled.
	// We expect normalized connections.
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnectionEgress}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// The runtime config file is created, but external IPs is disables.
	// There is no change in the state, so there are no changes to the connections
	s.setRuntimeConfig(types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled: "DISABLED",
			},
		},
	})

	assert.AssertExternalIps(s.T(), "DISABLED", collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// Back to using default behavior of external IPs disabled with no file.
	s.deleteFile(runtimeConfigFile)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFileInvalid() {
	// The runtime config file was deleted before starting collector.
	// Default configuration is external IPs disabled.
	// We expect normalized connections.
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnectionEgress}
	connectionSuccess := s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	// Testing an invalid configuration. There should not be a change in the configuration or reported connections
	invalidConfig := "asdf"
	s.writeRuntimeConfig(runtimeConfigFile, invalidConfig)
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	connectionSuccess = s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigNetworkIngress() {
	client, server := s.runBerserkerContainers()
	defer s.teardownBerserkerContainers()

	//	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	//	expectedConnections := []types.NetworkInfo{activeNormalizedConnectionEgress}
	//	s.Require().True(s.Sensor().ExpectSameElementsConnections(s.T(), server, 10*time.Second, expectedConnections...))

	s.setRuntimeConfig(types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled:   "ENABLED",
				Direction: "INGRESS",
			},
		},
	})

	common.Sleep(45 * time.Second)

	fmt.Println(s.Sensor().Connections(client))
	fmt.Println("===========")
	fmt.Println(s.Sensor().Connections(server))

	// assert.AssertExternalIps(s.T(), "ENABLED", collectorIP)
	// expectedConnections = append(expectedConnections, activeUnnormalizedConnectionEgress, inactiveNormalizedConnectionEgress)
	// common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	// s.Require().True(s.Sensor().ExpectSameElementsConnections(s.T(), client, 10*time.Second, expectedConnections...))
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigNetworkEgress() {
	assert.AssertNoRuntimeConfig(s.T(), collectorIP)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnectionEgress}
	s.Require().True(s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...))

	s.setRuntimeConfig(types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled:   "ENABLED",
				Direction: "EGRESS",
			},
		},
	})

	assert.AssertExternalIps(s.T(), "ENABLED", collectorIP)
	expectedConnections = append(expectedConnections, activeUnnormalizedConnectionEgress, inactiveNormalizedConnectionEgress)
	common.Sleep(3 * time.Second) // Sleep so that collector has a chance to report connections
	s.Require().True(s.Sensor().ExpectSameElementsConnections(s.T(), s.EgressClientContainer, 10*time.Second, expectedConnections...))
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigNetworkBoth() {

}
