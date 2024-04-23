package suites

import (
	"context"
	"fmt"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"

	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"
)

type RuntimeControlTestSuite struct {
	IntegrationTestSuiteBase
}

func (k *RuntimeControlTestSuite) SetupSuite() {
	// Ensure the collector pod gets deleted
	k.T().Cleanup(func() {
		exists, err := k.Executor().ContainerExists(executor.ContainerFilter{
			Name:      "collector",
			Namespace: collector.TEST_NAMESPACE,
		})
		k.Require().NoError(err)
		if exists {
			k.Collector().TearDown()
		}

		if k.sensor != nil {
			k.sensor.Stop()
		}

		k8sExecutor, ok := k.Executor().(*executor.K8sExecutor)
		if !ok {
			k.Require().FailNow("Incorrect executor type. got=%T, want=K8sExecutor", k.Executor())
		}

		k8sExecutor.RemoveNamespace(NAMESPACE)
	})

	// Start Sensor
	k.Sensor().Start()

	k.Sensor().OnCollectorRuntimeControlServiceConnect = func() {
		k.Sensor().SendRuntimeFilters(&sensorAPI.CollectorRuntimeConfigWithCluster{})
	}

	err := k.Collector().Setup(&collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_RUNTIME_FILTERS_ENABLED": "true",
		},
	})
	k.Require().NoError(err)
	err = k.Collector().Launch()
	k.Require().NoError(err)

	// wait for collector to be running
	k.watchPod("app=collector", collector.TEST_NAMESPACE, func() bool {
		return len(k.Collector().ContainerID()) != 0
	})
}

func (k *RuntimeControlTestSuite) TearDownSuite() {
}

func (k *RuntimeControlTestSuite) TestRuntimeControl() {
	ackReceived := k.Sensor().WaitForRuntimeFiltersAck(30)

	fmt.Printf("ackReceived: %q\n", ackReceived)

	k.Require().True(ackReceived)
}

func (k *RuntimeControlTestSuite) watchPod(selector string, namespace string, callback func() bool) {
	k8sExec := k.Executor().(*executor.K8sExecutor)

	timeout := int64(60)
	listOptions := metaV1.ListOptions{
		TimeoutSeconds: &timeout,
		LabelSelector:  selector,
	}
	watcher, err := k8sExec.ClientSet().CoreV1().Pods(namespace).Watch(context.Background(), listOptions)
	k.Require().NoError(err)

	for event := range watcher.ResultChan() {
		switch event.Type {
		case watch.Added:
		case watch.Modified:
			if callback() {
				return
			}
		default:
			// nothing to do here
		}
	}
}
