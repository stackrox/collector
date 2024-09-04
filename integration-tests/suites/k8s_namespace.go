package suites

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
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

type NamespaceTest struct {
	containerID       string
	expectecNamespace string
}

type K8sNamespaceTestSuite struct {
	suite.Suite
	k8sExecutor  *executor.K8sExecutor
	k8sCollector *collector.K8sCollectorManager
	sensor       *mock_sensor.MockSensor
	tests        []NamespaceTest
	collectorIP  string

	targetNamespaceWatcher watch.Interface
}

// Sensor returns the current mock sensor object, or initializes a new one
// if it is nil.
func (s *K8sNamespaceTestSuite) Sensor() *mock_sensor.MockSensor {
	if s.sensor == nil {
		s.sensor = mock_sensor.NewMockSensor(s.T().Name())
	}
	return s.sensor
}

func (s *K8sNamespaceTestSuite) Executor() *executor.K8sExecutor {
	if s.k8sExecutor == nil {
		exec, err := executor.NewK8sExecutor()
		s.Require().NoError(err)
		s.k8sExecutor = exec
	}
	return s.k8sExecutor
}

func (s *K8sNamespaceTestSuite) Collector() *collector.K8sCollectorManager {
	if s.k8sCollector == nil {
		s.k8sCollector = collector.NewK8sManager(*(s.Executor()), s.T().Name())
	}
	return s.k8sCollector
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

	err := k.Collector().Setup(&collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_RUNTIME_CONFIG_ENABLED": "true",
			"ROX_COLLECTOR_INTROSPECTION_ENABLE":   "true",
		},
	})
	k.Require().NoError(err)
	err = k.Collector().Launch()
	k.Require().NoError(err)

	// wait for collector to be running
	k.watchPod("app=collector", collector.TEST_NAMESPACE, func() bool {
		return len(k.Collector().ContainerID()) != 0
	})

	containerID := k.Collector().ContainerID()
	if len(containerID) == 0 {
		k.FailNow("Failed to get collector container ID")
	}

	// wait for the canary process to guarantee collector is started
	selfCheckOk := k.Sensor().WaitProcessesN(
		containerID, 30*time.Second, 1, func() {
			// Self-check process is not going to be sent via GRPC, instead
			// create at least one canary process to make sure everything is
			// fine.
			log.Info("spawn a canary process, container ID: %s", k.Collector().ContainerID())
			_, err = k.execPod("collector", collector.TEST_NAMESPACE, []string{"echo"})
			k.Require().NoError(err)
		})
	k.Require().True(selfCheckOk)

	k.collectorIP = k.getCollectorIP()
	k.tests = append(k.tests, NamespaceTest{
		containerID:       containerID,
		expectecNamespace: collector.TEST_NAMESPACE,
	})

	k.createTargetNamespace()
	nginxID := k.launchNginxPod()
	k.Require().Len(nginxID, 12)
	k.tests = append(k.tests, NamespaceTest{
		containerID:       nginxID,
		expectecNamespace: NAMESPACE,
	})
}

func (k *K8sNamespaceTestSuite) TearDownSuite() {
}

func (k *K8sNamespaceTestSuite) TestK8sNamespace() {
	// Sleep to ensure asynchronous container engine lookups have completed
	time.Sleep(10 * time.Second)
	baseUri := "http://" + k.collectorIP + ":8080/state/containers/"

	for _, tt := range k.tests {
		uri := baseUri + tt.containerID
		log.Info("Querying: %s\n", uri)
		resp, err := http.Get(uri)
		k.Require().NoError(err)
		k.Require().True(resp.StatusCode == 200)

		defer resp.Body.Close()
		raw, err := io.ReadAll(resp.Body)
		log.Info("Response: %s\n", raw)

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
		k.Require().Equal(namespace, tt.expectecNamespace)
	}
}

func (k *K8sNamespaceTestSuite) execPod(podName string, namespace string, command []string) (string, error) {
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

func (k *K8sNamespaceTestSuite) watchPod(selector string, namespace string, callback func() bool) {
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

func (k *K8sNamespaceTestSuite) getCollectorIP() string {
	pod, err := k.Executor().ClientSet().CoreV1().Pods(collector.TEST_NAMESPACE).Get(context.Background(), "collector", metaV1.GetOptions{})
	k.Require().NoError(err)

	return pod.Status.PodIP
}

func (k *K8sNamespaceTestSuite) createTargetNamespace() {
	_, err := k.Executor().CreateNamespace(NAMESPACE)
	k.Require().NoError(err)
	eventWatcher, err := k.Executor().CreateNamespaceEventWatcher(k.Collector().TestName(), NAMESPACE)
	k.Require().NoError(err)
	k.targetNamespaceWatcher = eventWatcher
}

func (k *K8sNamespaceTestSuite) teardownTargetNamespace() {
	if k.targetNamespaceWatcher != nil {
		k.targetNamespaceWatcher.Stop()
	}

	exists, _ := k.Executor().NamespaceExists(NAMESPACE)
	if exists {
		k.Executor().RemoveNamespace(NAMESPACE)
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
