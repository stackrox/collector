package k8s

import (
	"encoding/json"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/log"

	coreV1 "k8s.io/api/core/v1"
	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

const (
	EXT_IP_ENABLE = `
networking:
  externalIps:
    enable: true
`

	EXT_IP_DISABLE = `
networking:
  externalIps:
    enable: false
`

	CONFIG_MAP_NAME = "collector-config"
)

type ConfigQueryResponse struct {
	Networking struct {
		ExternalIps struct {
			Enable bool
		}
	}
}

type K8sConfigReloadTestSuite struct {
	K8sTestSuiteBase
}

func (k *K8sConfigReloadTestSuite) SetupSuite() {
	k.T().Cleanup(func() {
		k.Sensor().Stop()
		k.teardownTargetNamespace()
	})

	k.Sensor().Start()
}

func (k *K8sConfigReloadTestSuite) AfterTest(suiteName, testName string) {
	k.StopCollector()
	k.Executor().RemoveConfigMap(k.TestNamespace(), CONFIG_MAP_NAME)
}

func (k *K8sConfigReloadTestSuite) TestCreateConfigurationAfterStart() {
	k.StartCollector(&collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_INTROSPECTION_ENABLE": "true",
		},
	})

	log.Info("Checking runtime configuration is not in use")
	k.assertNoRuntimeConfig()

	log.Info("Checking external IPs is enabled")
	configMap := coreV1.ConfigMap{
		ObjectMeta: metaV1.ObjectMeta{
			Name:      CONFIG_MAP_NAME,
			Namespace: k.TestNamespace(),
		},
		Data: map[string]string{
			"runtime_config.yaml": EXT_IP_ENABLE,
		},
	}
	k.createConfigMap(&configMap)
	k.AssertExternalIps(true)

	log.Info("Checking external IPs is disabled")
	configMap.Data["runtime_config.yaml"] = EXT_IP_DISABLE
	k.updateConfigMap(&configMap)
	k.AssertExternalIps(false)

	log.Info("Checking runtime configuration is not in use")
	k.deleteConfigMap(CONFIG_MAP_NAME)
	k.assertNoRuntimeConfig()
}

func (k *K8sConfigReloadTestSuite) TestConfigurationReload() {
	log.Info("Checking external IPs is enabled")
	configMap := coreV1.ConfigMap{
		ObjectMeta: metaV1.ObjectMeta{
			Name:      CONFIG_MAP_NAME,
			Namespace: k.TestNamespace(),
		},
		Data: map[string]string{
			"runtime_config.yaml": EXT_IP_ENABLE,
		},
	}
	k.createConfigMap(&configMap)

	k.StartCollector(&collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_INTROSPECTION_ENABLE": "true",
		},
	})
	k.AssertExternalIps(true)

	log.Info("Checking external IPs is disabled")
	configMap.Data["runtime_config.yaml"] = EXT_IP_DISABLE
	k.updateConfigMap(&configMap)
	k.AssertExternalIps(false)
}

func (k *K8sConfigReloadTestSuite) AssertExternalIps(enable bool) {
	tick := time.Tick(10 * time.Second)
	timer := time.After(3 * time.Minute)

	for {
		select {
		case <-tick:
			log.Info("Querying: /state/config")
			body, err := k.Collector().IntrospectionQuery("/state/config")
			k.Require().NoError(err)
			log.Info("Response: %q", body)

			var response ConfigQueryResponse
			err = json.Unmarshal(body, &response)
			k.Require().NoError(err)

			if response.Networking.ExternalIps.Enable == enable {
				// Test succeeded
				return
			}

		case <-timer:
			k.FailNow("Runtime configuration was not updated")
		}
	}
}

func (k *K8sConfigReloadTestSuite) assertNoRuntimeConfig() {
	tick := time.Tick(10 * time.Second)
	timer := time.After(3 * time.Minute)

	for {
		select {
		case <-tick:
			log.Info("Querying: /state/config")
			body, err := k.Collector().IntrospectionQuery("/state/config")
			k.Require().NoError(err)
			log.Info("Response: %q", body)

			if strings.TrimSpace(string(body)) == "{}" {
				// Test succeeded
				return
			}

		case <-timer:
			k.FailNow("Runtime configuration was not updated")
		}
	}

}
