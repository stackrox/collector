package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type NetworkStressSuite struct {
	IntegrationTestSuiteBase
	serverID string
	clientID string
}

func (s *NetworkStressSuite) SetupSuite() {
	s.RegisterCleanup("berserker-client", "berserker-server")
	s.StartContainerStats()
	image_store := config.Images()

	berserker_img := image_store.QaImageByKey("performance-berserker")

	images := []string{
		berserker_img,
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			"scrapeInterval": 5,
		},
		Env: map[string]string{
			"ROX_AFTERFLOW_PERIOD": "0",
			"ROX_ENABLE_AFTERFLOW": "false",
		},
	}

	s.StartCollector(false, &collectorOptions)

	berserkerID, err := s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "berserker-server",
		Image: berserker_img,
		Env: map[string]string{
			"BERSERKER__WORKLOAD__NCONNECTIONS": "100",
			"RUST_BACKTRACE":                    "full",
		},
		Command:     []string{"/etc/berserker/network/server.toml"},
		NetworkMode: "host",
		Privileged:  true,
	})
	s.Require().NoError(err)
	s.serverID = berserkerID

	berserkerID, err = s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "berserker-client",
		Image: berserker_img,
		Env: map[string]string{
			"BERSERKER__WORKLOAD__NCONNECTIONS": "100",
			"RUST_BACKTRACE":                    "full",
		},
		Command:     []string{"/etc/berserker/network/client.toml"},
		NetworkMode: "host",
		Privileged:  true,
	})
	s.Require().NoError(err)
	s.clientID = berserkerID
}

func (s *NetworkStressSuite) TestNetworkStress() {
	common.Sleep(100 * time.Second)
}
