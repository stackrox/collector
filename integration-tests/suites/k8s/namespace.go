package k8s

import (
	"encoding/json"
	"fmt"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
	"github.com/stackrox/collector/integration-tests/pkg/log"

	coreV1 "k8s.io/api/core/v1"
	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

type NamespaceTest struct {
	containerID       string
	expectedNamespace string
}

type K8sNamespaceTestSuite struct {
	K8sTestSuiteBase
	tests []NamespaceTest
}

func (k *K8sNamespaceTestSuite) SetupSuite() {
	// Ensure the collector pod gets deleted
	k.T().Cleanup(func() {
		k.Collector().TearDown()

		if k.sensor != nil {
			k.sensor.Stop()
		}

		k.teardownNginxPod()
		k.teardownTargetNamespace()
	})

	// Start Sensor
	k.Sensor().Start()

	k.StartCollector(&collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_RUNTIME_CONFIG_ENABLED": "true",
			"ROX_COLLECTOR_INTROSPECTION_ENABLE":   "true",
		},
	})

	k.tests = append(k.tests, NamespaceTest{
		containerID:       k.Collector().ContainerID(),
		expectedNamespace: collector.TEST_NAMESPACE,
	})

	k.createTargetNamespace()
	nginxID := k.launchNginxPod()
	k.Require().Len(nginxID, 12)
	k.tests = append(k.tests, NamespaceTest{
		containerID:       nginxID,
		expectedNamespace: NAMESPACE,
	})
}

func (k *K8sNamespaceTestSuite) TestK8sNamespace() {
	for _, tt := range k.tests {
		endpoint := fmt.Sprintf("/state/containers/%s", tt.containerID)
		log.Info("Querying: %s", endpoint)
		raw, err := collector.IntrospectionQuery(k.Collector().IP(), endpoint)
		k.Require().NoError(err)
		log.Info("Response: %s", raw)

		var body map[string]interface{}
		err = json.Unmarshal(raw, &body)
		k.Require().NoError(err)

		cIdInterface, ok := body["container_id"]
		k.Require().True(ok)
		cId, ok := cIdInterface.(string)
		k.Require().True(ok)
		k.Require().Equal(cId, tt.containerID)

		namespaceInterface, ok := body["namespace"]
		k.Require().True(ok)
		namespace, ok := namespaceInterface.(string)
		k.Require().True(ok)
		k.Require().Equal(namespace, tt.expectedNamespace)
	}
}

func (k *K8sNamespaceTestSuite) launchNginxPod() string {
	pod := &coreV1.Pod{
		ObjectMeta: metaV1.ObjectMeta{
			Name:      "nginx",
			Namespace: NAMESPACE,
			Labels: map[string]string{
				"app": "nginx",
			},
		},
		Spec: coreV1.PodSpec{
			Containers: []coreV1.Container{
				{Name: "nginx", Image: "nginx:1.24.0"},
			},
		},
	}

	_, err := k.Executor().CreatePod(NAMESPACE, pod)
	k.Require().NoError(err)

	// Wait for nginx pod to start up
	pf := executor.ContainerFilter{
		Name:      "nginx",
		Namespace: NAMESPACE,
	}

	log.Info("Waiting for nginx to start running")
	k.watchPod("app=nginx", NAMESPACE, func() bool {
		return k.Executor().PodContainerID(pf) != ""
	})

	return k.Executor().PodContainerID(pf)
}

func (k *K8sNamespaceTestSuite) teardownNginxPod() {
	nginxPodFilter := executor.ContainerFilter{
		Name:      "nginx",
		Namespace: NAMESPACE,
	}
	exists, _ := k.Executor().PodExists(nginxPodFilter)

	if exists {
		err := k.Executor().CapturePodConfiguration(k.Collector().TestName(), NAMESPACE, "nginx")
		k.Require().NoError(err)
		k.Executor().RemovePod(nginxPodFilter)
	}
}
