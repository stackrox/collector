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
		CloseTimestamp: types.NotNilTimestamp,
	}

	inactiveUnnormalizedConnectionEgress = types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", externalIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NotNilTimestamp,
	}

	runtimeConfigDir  = "/tmp/collector-test"
	runtimeConfigFile = filepath.Join(runtimeConfigDir, "/runtime_config.yaml")
	collectorIP       = "localhost"

	ingressIP   = "223.42.0.1"
	ingressPort = 1337
)

type RuntimeConfigFileTestSuite struct {
	IntegrationTestSuiteBase
	EgressClientContainer string
}

func (s *RuntimeConfigFileTestSuite) writeRuntimeConfig(runtimeConfigFile string, configStr string) {
	err := os.WriteFile(runtimeConfigFile, []byte(configStr), 0666)
	s.Require().NoError(err)
}

func (s *RuntimeConfigFileTestSuite) setRuntimeConfig(config types.RuntimeConfig) {
	s.writeRuntimeConfig(runtimeConfigFile, config.String())
}

func (s *RuntimeConfigFileTestSuite) runNetworkDirectionContainers() (client, server string) {
	serverCmd := fmt.Sprintf("/scripts/prepare-tap.sh -a %s -o && nc -lk %s %d", ingressIP, ingressIP, ingressPort)
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:        "external-connection-ingress-server",
			Image:       config.Images().QaImageByKey("performance-berserker"),
			Privileged:  true,
			NetworkMode: "host",
			Entrypoint: []string{
				"sh", "-c", serverCmd,
			},
		},
	)
	s.Require().NoError(err)
	server = common.ContainerShortID(containerID)

	clientCmd := fmt.Sprintf("sleep 20; while true; do nc -zv %s %d; sleep 60; done", ingressIP, ingressPort)
	containerID, err = s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:        "external-connection-ingress-client",
			Image:       config.Images().QaImageByKey("performance-berserker"),
			Privileged:  true,
			NetworkMode: "host",
			Entrypoint: []string{
				"sh", "-c", clientCmd,
			},
		},
	)
	s.Require().NoError(err)
	client = common.ContainerShortID(containerID)

	return client, server
}

func (s *RuntimeConfigFileTestSuite) teardownNetworkDirectionContainers() {
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
	client, server := s.runNetworkDirectionContainers()
	defer s.teardownNetworkDirectionContainers()

	config := types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled:   "ENABLED",
				Direction: "INGRESS",
			},
		},
	}

	s.setRuntimeConfig(config)
	assert.AssertRuntimeConfig(s.T(), collectorIP, config)

	// Expect both open and close events for the non-aggregated
	// ingress connection. If Collector is aggregating to 255.255.255.255
	// this will fail.
	// We are not concerned with event ordering in this test.
	expectedIngressConnections := []types.NetworkInfo{
		{
			LocalAddress:   fmt.Sprintf(":%d", ingressPort),
			RemoteAddress:  ingressIP,
			Role:           "ROLE_SERVER",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NotNilTimestamp,
		},
		{
			LocalAddress:   fmt.Sprintf(":%d", ingressPort),
			RemoteAddress:  ingressIP,
			Role:           "ROLE_SERVER",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NilTimestamp,
		},
	}

	expectedEgressConnections := []types.NetworkInfo{
		{
			LocalAddress:   "",
			RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, ingressPort),
			Role:           "ROLE_CLIENT",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NotNilTimestamp,
		},
		{
			LocalAddress:   "",
			RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, ingressPort),
			Role:           "ROLE_CLIENT",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NilTimestamp,
		},
	}

	s.Sensor().ExpectConnections(s.T(), client, 30*time.Second, expectedEgressConnections...)
	s.Sensor().ExpectConnections(s.T(), server, 30*time.Second, expectedIngressConnections...)
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigNetworkEgress() {
	client, server := s.runNetworkDirectionContainers()
	defer s.teardownNetworkDirectionContainers()

	config := types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled:   "ENABLED",
				Direction: "EGRESS",
			},
		},
	}

	s.setRuntimeConfig(config)
	assert.AssertRuntimeConfig(s.T(), collectorIP, config)

	// Expect both open and close events for the non-aggregated
	// egress connection. If Collector is aggregating to 255.255.255.255
	// this will fail.
	// We are not concerned with event ordering in this test.
	expectedEgressConnections := []types.NetworkInfo{
		{
			LocalAddress:   "",
			RemoteAddress:  fmt.Sprintf("%s:%d", ingressIP, ingressPort),
			Role:           "ROLE_CLIENT",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NotNilTimestamp,
		},
		{
			LocalAddress:   "",
			RemoteAddress:  fmt.Sprintf("%s:%d", ingressIP, ingressPort),
			Role:           "ROLE_CLIENT",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NilTimestamp,
		},
	}

	expectedIngressConnections := []types.NetworkInfo{
		{
			LocalAddress:   fmt.Sprintf(":%d", ingressPort),
			RemoteAddress:  normalizedIp,
			Role:           "ROLE_SERVER",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NotNilTimestamp,
		},
		{
			LocalAddress:   fmt.Sprintf(":%d", ingressPort),
			RemoteAddress:  normalizedIp,
			Role:           "ROLE_SERVER",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NilTimestamp,
		},
	}

	s.Require().True(s.Sensor().ExpectConnections(s.T(), client, 30*time.Second, expectedEgressConnections...))
	s.Require().True(s.Sensor().ExpectConnections(s.T(), server, 30*time.Second, expectedIngressConnections...))
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigNetworkBoth() {
	client, server := s.runNetworkDirectionContainers()
	defer s.teardownNetworkDirectionContainers()

	config := types.RuntimeConfig{
		Networking: types.NetworkConfig{
			ExternalIps: types.ExternalIpsConfig{
				Enabled:   "ENABLED",
				Direction: "BOTH",
			},
		},
	}

	s.setRuntimeConfig(config)
	assert.AssertRuntimeConfig(s.T(), collectorIP, config)

	// Expect both open and close events for the non-aggregated
	// egress and ingress connections. If Collector is aggregating to 255.255.255.255
	// this will fail.
	// We are not concerned with event ordering in this test.
	expectedEgressConnections := []types.NetworkInfo{
		{
			LocalAddress:   "",
			RemoteAddress:  fmt.Sprintf("%s:%d", ingressIP, ingressPort),
			Role:           "ROLE_CLIENT",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NotNilTimestamp,
		},
		{
			LocalAddress:   "",
			RemoteAddress:  fmt.Sprintf("%s:%d", ingressIP, ingressPort),
			Role:           "ROLE_CLIENT",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NilTimestamp,
		},
	}

	expectedIngressConnections := []types.NetworkInfo{
		{
			LocalAddress:   fmt.Sprintf(":%d", ingressPort),
			RemoteAddress:  ingressIP,
			Role:           "ROLE_SERVER",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NotNilTimestamp,
		},
		{
			LocalAddress:   fmt.Sprintf(":%d", ingressPort),
			RemoteAddress:  ingressIP,
			Role:           "ROLE_SERVER",
			SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
			CloseTimestamp: types.NilTimestamp,
		},
	}

	s.Require().True(s.Sensor().ExpectConnections(s.T(), client, 30*time.Second, expectedEgressConnections...))
	s.Require().True(s.Sensor().ExpectConnections(s.T(), server, 30*time.Second, expectedIngressConnections...))
}
