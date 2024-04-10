package suites

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/executor"

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
	IntegrationTestSuiteBase
	tests       []NamespaceTest
	collectorIP string
}

func (k *K8sNamespaceTestSuite) SetupSuite() {
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

		nginxPodFilter := executor.ContainerFilter{
			Name:      "nginx",
			Namespace: NAMESPACE,
		}
		exists, err = k.Executor().ContainerExists(nginxPodFilter)
		k.Require().NoError(err)

		if exists {
			k.Executor().RemoveContainer(nginxPodFilter)
		}

		k8sExecutor, ok := k.Executor().(*executor.K8sExecutor)
		if !ok {
			k.Require().FailNow("Incorrect executor type. got=%T, want=K8sExecutor", k.Executor())
		}

		exists, err = k8sExecutor.NamespaceExists(NAMESPACE)
		k.Require().NoError(err)
		if exists {
			k8sExecutor.RemoveNamespace(NAMESPACE)
		}
	})

	// Start Sensor
	k.Sensor().Start()

	err := k.Collector().Setup(&collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_RUNTIME_FILTERS_ENABLED": "true",
			"ROX_COLLECTOR_INTROSPECTION_ENABLE":    "true",
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
			fmt.Printf("Spawn a canary process, container ID: %s\n", k.Collector().ContainerID())
			_, err = k.execPod("collector", collector.TEST_NAMESPACE, []string{"echo"})
			k.Require().NoError(err)
		})
	k.Require().True(selfCheckOk)

	k.collectorIP = k.getCollectorIP()
	k.tests = append(k.tests, NamespaceTest{
		containerID:       containerID,
		expectecNamespace: collector.TEST_NAMESPACE,
	})

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
	baseUri := "http://" + k.collectorIP + ":8080/state/containers/"

	for _, tt := range k.tests {
		uri := baseUri + tt.containerID
		fmt.Printf("Querying: %s\n", uri)
		resp, err := http.Get(uri)
		k.Require().NoError(err)
		k.Require().True(resp.StatusCode == 200)

		defer resp.Body.Close()
		raw, err := io.ReadAll(resp.Body)
		fmt.Printf("Response: %s\n", raw)

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
	k8sExec, ok := k.executor.(*executor.K8sExecutor)
	if !ok {
		return "", fmt.Errorf("Expected k8s executor, got=%T", k.executor)
	}

	req := k8sExec.ClientSet().CoreV1().RESTClient().Post().Resource("Pods").Name(podName).Namespace(namespace).SubResource("exec")
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

func (k *K8sNamespaceTestSuite) getCollectorIP() string {
	k8sExec := k.Executor().(*executor.K8sExecutor)
	pod, err := k8sExec.ClientSet().CoreV1().Pods(collector.TEST_NAMESPACE).Get(context.Background(), "collector", metaV1.GetOptions{})
	k.Require().NoError(err)

	return pod.Status.PodIP
}

func (k *K8sNamespaceTestSuite) launchNginxPod() string {
	// Create a namespace for the pod
	k8sExec := k.Executor().(*executor.K8sExecutor)

	_, err := k8sExec.CreateNamespace(NAMESPACE)

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

	_, err = k8sExec.CreatePod(NAMESPACE, pod)
	k.Require().NoError(err)

	// Wait for nginx pod to start up
	pf := executor.ContainerFilter{
		Name:      "nginx",
		Namespace: NAMESPACE,
	}

	fmt.Println("Waiting for nginx to start running")
	k.watchPod("app=nginx", NAMESPACE, func() bool {
		return k8sExec.ContainerID(pf) != ""
	})

	return k8sExec.ContainerID(pf)
}
