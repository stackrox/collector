package k8s

import (
	"context"
	"os"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/mock_sensor"
	"github.com/stretchr/testify/suite"

	coreV1 "k8s.io/api/core/v1"
	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/remotecommand"
)

const (
	NAMESPACE = "target-namespace"
)

type K8sTestSuiteBase struct {
	suite.Suite
	k8sExecutor  *executor.K8sExecutor
	k8sCollector *collector.K8sCollectorManager
	sensor       *mock_sensor.MockSensor

	targetNamespaceWatcher watch.Interface
}

// Sensor returns the current mock sensor object, or initializes a new one
// if it is nil.
func (s *K8sTestSuiteBase) Sensor() *mock_sensor.MockSensor {
	if s.sensor == nil {
		s.sensor = mock_sensor.NewMockSensor(s.T().Name())
	}
	return s.sensor
}

func (s *K8sTestSuiteBase) Executor() *executor.K8sExecutor {
	if s.k8sExecutor == nil {
		exec, err := executor.NewK8sExecutor()
		s.Require().NoError(err)
		s.k8sExecutor = exec
	}
	return s.k8sExecutor
}

func (s *K8sTestSuiteBase) Collector() *collector.K8sCollectorManager {
	if s.k8sCollector == nil {
		s.k8sCollector = collector.NewK8sManager(*(s.Executor()), s.T().Name())
	}
	return s.k8sCollector
}

// StartCollector will start the collector pod
func (s *K8sTestSuiteBase) StartCollector(opts *collector.StartupOptions) {
	s.Require().NoError(s.Collector().Setup(opts))
	s.Require().NoError(s.Collector().Launch())

	// wait for collector to be running
	s.watchPod("app=collector", s.TestNamespace(), func() bool {
		return len(s.Collector().ContainerID()) != 0
	})

	containerID := s.Collector().ContainerID()
	if len(containerID) == 0 {
		s.FailNow("Failed to get collector container ID")
	}

	// wait for the canary process to guarantee collector is started
	selfCheckOk := s.Sensor().WaitProcessesN(
		containerID, 30*time.Second, 1, func() {
			// Self-check process is not going to be sent via GRPC, instead
			// create at least one canary process to make sure everything is
			// fine.
			log.Info("spawn a canary process, container ID: %s", containerID)
			_, err := s.execPod("collector", s.TestNamespace(), []string{"echo"})
			s.Require().NoError(err)
		})
	s.Require().True(selfCheckOk)
}

func (s *K8sTestSuiteBase) StopCollector() {
	s.Collector().TearDown()
	s.k8sCollector = nil
}

func (k *K8sTestSuiteBase) execPod(podName string, namespace string, command []string) (string, error) {
	req := k.Executor().ClientSet().CoreV1().RESTClient().Post().Resource("Pods").Name(podName).Namespace(namespace).SubResource("exec")
	option := &coreV1.PodExecOptions{
		Command: command,
		Stdin:   false,
		Stdout:  true,
		Stderr:  true,
		TTY:     false,
	}
	req.VersionedParams(option, scheme.ParameterCodec)

	config, err := rest.InClusterConfig()
	if err != nil {
		return "", err
	}

	exec, err := remotecommand.NewSPDYExecutor(config, "POST", req.URL())
	if err != nil {
		return "", err
	}

	err = exec.StreamWithContext(context.Background(), remotecommand.StreamOptions{
		Stdin:  nil,
		Stdout: os.Stdout,
		Stderr: os.Stderr,
	})

	if err != nil {
		return "", err
	}

	return "", nil
}

func (k *K8sTestSuiteBase) watchPod(selector string, namespace string, callback func() bool) {
	timeout := int64(60)
	listOptions := metaV1.ListOptions{
		TimeoutSeconds: &timeout,
		LabelSelector:  selector,
	}
	watcher, err := k.Executor().ClientSet().CoreV1().Pods(namespace).Watch(context.Background(), listOptions)
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

func (k *K8sTestSuiteBase) createTargetNamespace() {
	_, err := k.Executor().CreateNamespace(NAMESPACE)
	k.Require().NoError(err)
	eventWatcher, err := k.Executor().CreateNamespaceEventWatcher(k.Collector().TestName(), NAMESPACE)
	k.Require().NoError(err)
	k.targetNamespaceWatcher = eventWatcher
}

func (k *K8sTestSuiteBase) teardownTargetNamespace() {
	if k.targetNamespaceWatcher != nil {
		k.targetNamespaceWatcher.Stop()
	}

	exists, _ := k.Executor().NamespaceExists(NAMESPACE)
	if exists {
		k.Executor().RemoveNamespace(NAMESPACE)
	}
}

func (k *K8sTestSuiteBase) createConfigMap(configMap *coreV1.ConfigMap) {
	_, err := k.Executor().CreateConfigMap(k.TestNamespace(), configMap)
	k.Require().NoError(err)
}

func (k *K8sTestSuiteBase) updateConfigMap(configMap *coreV1.ConfigMap) {
	_, err := k.Executor().UpdateConfigMap(k.TestNamespace(), configMap)
	k.Require().NoError(err)
}

func (k *K8sTestSuiteBase) deleteConfigMap(name string) {
	err := k.Executor().RemoveConfigMap(k.TestNamespace(), name)
	k.Require().NoError(err)
}

func (k *K8sTestSuiteBase) Namespace() string {
	return NAMESPACE
}

func (k *K8sTestSuiteBase) TestNamespace() string {
	return collector.TEST_NAMESPACE
}
