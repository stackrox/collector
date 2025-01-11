package k8s

import (
	"github.com/stackrox/collector/integration-tests/pkg/assert"
	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/types"

	coreV1 "k8s.io/api/core/v1"
	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

var (
	EXT_IP_ENABLE  string
	EXT_IP_DISABLE string

	CONFIG_MAP_NAME = "collector-config"
)

func init() {
	EXT_IP_ENABLE, _ = types.GetRuntimeConfigEnabledStr("ENABLED")
	EXT_IP_DISABLE, _ = types.GetRuntimeConfigEnabledStr("DISABLED")
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
	assert.AssertNoRuntimeConfig(k.T(), k.Collector().IP())

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
	assert.AssertExternalIps(k.T(), "ENABLED", k.Collector().IP())

	log.Info("Checking external IPs is disabled")
	configMap.Data["runtime_config.yaml"] = EXT_IP_DISABLE
	k.updateConfigMap(&configMap)
	assert.AssertExternalIps(k.T(), "DISABLED", k.Collector().IP())

	log.Info("Checking runtime configuration is not in use")
	k.deleteConfigMap(CONFIG_MAP_NAME)
	assert.AssertNoRuntimeConfig(k.T(), k.Collector().IP())

	log.Info("Checking external IPs is enabled again")
	configMap.Data["runtime_config.yaml"] = EXT_IP_ENABLE
	k.createConfigMap(&configMap)
	assert.AssertExternalIps(k.T(), "ENABLED", k.Collector().IP())
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
	assert.AssertExternalIps(k.T(), "ENABLED", k.Collector().IP())

	log.Info("Checking external IPs is disabled")
	configMap.Data["runtime_config.yaml"] = EXT_IP_DISABLE
	k.updateConfigMap(&configMap)
	assert.AssertExternalIps(k.T(), "DISABLED", k.Collector().IP())
}
