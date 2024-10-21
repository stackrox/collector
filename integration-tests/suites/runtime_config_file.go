package suites

import (
	"fmt"
	"path/filepath"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
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

func (s *RuntimeConfigFileTestSuite) setRuntimeConfig(runtimeConfigFile string, configStr string) {
	_, err := s.execContainer("collector", []string{"/bin/bash", "-c", "echo '" + configStr + "' > " + runtimeConfigFile})
	s.Require().NoError(err)
}

func (s *RuntimeConfigFileTestSuite) setExternalIpsEnable(runtimeConfigFile string, enable bool) {
	configStr := "networking:\n  externalIps:\n    enable: " + strconv.FormatBool(enable)
	s.setRuntimeConfig(runtimeConfigFile, configStr)
}

// Launches collector and creates the directory for runtime configuration.
// Writes various things to the file for runtime configuration and checks
// the config introspection endpoint to make sure that the configuration is
// correct.
func (s *RuntimeConfigFileTestSuite) SetupSuite() {
	s.RegisterCleanup("external-connection")

	s.StartContainerStats()

	normalizedIp := "255.255.255.255"
	externalIp := "8.8.8.8"
	serverPort := 53
	externalUrl := fmt.Sprintf("http://%s:%d", externalIp, serverPort)
	image_store := config.Images()
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    "external-connection",
			Image:   image_store.QaImageByKey("qa-alpine-curl"),
			Command: []string{"sh", "-c", "while true; do curl " + externalUrl + "; sleep 1; done"},
		})
	s.Require().NoError(err)
	clientContainer := common.ContainerShortID(containerID)

	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_AFTERGLOW_PERIOD":               "2",
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

	activeNormalizedConnection := types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", normalizedIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NilTimestamp,
	}

	activeUnnormalizedConnection := types.NetworkInfo{
		LocalAddress:   "",
		RemoteAddress:  fmt.Sprintf("%s:%d", externalIp, serverPort),
		Role:           "ROLE_CLIENT",
		SocketFamily:   "SOCKET_FAMILY_UNKNOWN",
		CloseTimestamp: types.NilTimestamp,
	}

	inactiveNormalizedConnection := activeNormalizedConnection
	inactiveNormalizedConnection.CloseTimestamp = "Not nill time"

	inactiveUnnormalizedConnection := activeUnnormalizedConnection
	inactiveUnnormalizedConnection.CloseTimestamp = "Not nill time"

	s.StartCollector(false, &collectorOptions)
	runtimeConfigDir := "/etc/stackrox"
	runtimeConfigFile := filepath.Join(runtimeConfigDir, "/runtime_config.yaml")
	s.createDir(runtimeConfigDir)

	runtimeConfigSuccess := introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
	expectedConnections := []types.NetworkInfo{activeNormalizedConnection}
	connectionSuccess := s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, true)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsTrue)
	s.Require().True(runtimeConfigSuccess)
	expectedConnections = append(expectedConnections, activeUnnormalizedConnection, inactiveNormalizedConnection)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, false)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
	expectedConnections = append(expectedConnections, activeNormalizedConnection, inactiveUnnormalizedConnection)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	s.deleteFile(runtimeConfigFile)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, true)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsTrue)
	s.Require().True(runtimeConfigSuccess)
	expectedConnections = append(expectedConnections, activeUnnormalizedConnection, inactiveNormalizedConnection)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	s.deleteFile(runtimeConfigFile)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 60*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
	expectedConnections = append(expectedConnections, activeNormalizedConnection, inactiveUnnormalizedConnection)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	s.setExternalIpsEnable(runtimeConfigFile, false)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)

	invalidConfig := "asdf"
	s.setRuntimeConfig(runtimeConfigFile, invalidConfig)
	runtimeConfigSuccess = introspection_endpoints.ExpectRuntimeConfig(s.T(), 30*time.Second, externalIpsFalse)
	s.Require().True(runtimeConfigSuccess)
	connectionSuccess = s.Sensor().ExpectExactConnections(s.T(), clientContainer, 10*time.Second, expectedConnections...)
	s.Require().True(connectionSuccess)
}

func (s *RuntimeConfigFileTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("external-connection")
	s.WritePerfResults()
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFile() {
}
