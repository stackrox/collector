package executor

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"strings"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	coreV1 "k8s.io/api/core/v1"
	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
)

const (
	TESTS_NAMESPACE = "collector-tests"
)

type K8sExecutor struct {
	clientset *kubernetes.Clientset
}

func newK8sExecutor() (*K8sExecutor, error) {
	fmt.Println("Creating k8s configuration")
	config, err := rest.InClusterConfig()
	if err != nil {
		fmt.Printf("Error: Failed to get cluster config: %s\n", err)
		return nil, err
	}

	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		fmt.Printf("Error: Failed to create client: %s", err)
		return nil, err
	}

	k8s := &K8sExecutor{
		clientset: clientset,
	}
	return k8s, nil
}

func (e *K8sExecutor) CopyFromHost(src string, dst string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) PullImage(image string) error {
	return fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) IsContainerRunning(podName string) (bool, error) {
	pod, err := e.clientset.CoreV1().Pods(TESTS_NAMESPACE).Get(context.Background(), podName, metaV1.GetOptions{})
	if err != nil {
		return false, err
	}

	if pod == nil || pod.Status.Phase != coreV1.PodRunning {
		return false, nil
	}

	return pod.Status.ContainerStatuses[0].Ready, nil
}

func (e *K8sExecutor) ContainerID(podFilter ContainerFilter) string {
	pod, err := e.ClientSet().CoreV1().Pods(podFilter.Namespace).Get(context.Background(), podFilter.Name, metaV1.GetOptions{})
	if err != nil {
		fmt.Printf("%s\n", err)
		return ""
	}

	if len(pod.Status.ContainerStatuses) != 1 {
		return ""
	}

	/*
	 * The format extracted from the following line looks something like this:
	 *    containerd://01e8c0454972a6b22b2e8ff7bf5a7d011e7dc7c0cde95c468a823b7085669a36
	 */
	containerID := pod.Status.ContainerStatuses[0].ContainerID
	if len(containerID) < 12 {
		fmt.Printf("Invalid container ID: %q\n", containerID)
		return ""
	}

	i := strings.LastIndex(containerID, "/")
	if i == -1 {
		fmt.Printf("Invalid container ID: %q\n", containerID)
		return ""
	}

	return common.ContainerShortID(containerID[i+1:])
}

func (e *K8sExecutor) ContainerExists(podFilter ContainerFilter) (bool, error) {
	pod, err := e.clientset.CoreV1().Pods(podFilter.Namespace).Get(context.Background(), podFilter.Name, metaV1.GetOptions{})
	if err != nil {
		return false, err
	}

	return pod != nil, nil
}

func (e *K8sExecutor) ExitCode(podFilter ContainerFilter) (int, error) {
	pod, err := e.clientset.CoreV1().Pods(podFilter.Namespace).Get(context.Background(), podFilter.Name, metaV1.GetOptions{})
	if err != nil {
		return -1, err
	}

	if pod == nil {
		return -1, fmt.Errorf("pod does not exist")
	}

	terminated := pod.Status.ContainerStatuses[0].State.Terminated
	if terminated == nil {
		return -1, fmt.Errorf("failed to get termination status")
	}

	return int(terminated.ExitCode), nil
}

func (e *K8sExecutor) Exec(args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) ExecWithStdin(pipedContent string, args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) ExecWithoutRetry(args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) KillContainer(name string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) RemoveContainer(podFilter ContainerFilter) (string, error) {
	err := e.clientset.CoreV1().Pods(podFilter.Namespace).Delete(context.Background(), podFilter.Name, metaV1.DeleteOptions{})
	return "", err
}

func (e *K8sExecutor) StopContainer(name string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) CreateNamespace(ns string) (*coreV1.Namespace, error) {
	meta := metaV1.ObjectMeta{Name: ns}
	return e.clientset.CoreV1().Namespaces().Create(context.Background(), &coreV1.Namespace{ObjectMeta: meta}, metaV1.CreateOptions{})
}

func (e *K8sExecutor) NamespaceExists(ns string) (bool, error) {
	res, err := e.clientset.CoreV1().Namespaces().Get(context.Background(), ns, metaV1.GetOptions{})
	if err != nil {
		return false, err
	}

	return res != nil, nil
}

func (e *K8sExecutor) RemoveNamespace(ns string) error {
	return e.clientset.CoreV1().Namespaces().Delete(context.Background(), ns, metaV1.DeleteOptions{})
}

func (e *K8sExecutor) CreatePod(ns string, pod *coreV1.Pod) (*coreV1.Pod, error) {
	return e.clientset.CoreV1().Pods(ns).Create(context.Background(), pod, metaV1.CreateOptions{})
}

func (e *K8sExecutor) ClientSet() *kubernetes.Clientset {
	return e.clientset
}

func (e *K8sExecutor) CapturePodConfiguration(testName, ns, podName string) error {
	pod, err := e.clientset.CoreV1().Pods(ns).Get(context.Background(), podName, metaV1.GetOptions{})
	if err != nil {
		return err
	}

	logFile, err := common.PrepareLog(testName, ns+"-"+podName+"-config.json")
	if err != nil {
		return err
	}
	defer logFile.Close()

	podJson, err := json.Marshal(pod)
	if err != nil {
		return err
	}

	_, err = logFile.Write(podJson)
	if err != nil {
		return err
	}

	return nil
}

func (e *K8sExecutor) CreateNamespaceEventWatcher(testName, ns string) (watch.Interface, error) {
	watcher, err := e.clientset.CoreV1().Events(ns).Watch(context.Background(), metaV1.ListOptions{})
	if err != nil {
		return nil, err
	}

	logFile, err := common.PrepareLog(testName, ns+"-events.jsonl")
	if err != nil {
		return nil, err
	}

	go func(watcher watch.Interface, logFile *os.File) {
		defer logFile.Close()

		for event := range watcher.ResultChan() {
			eventJson, err := json.Marshal(event)
			if err != nil {
				fmt.Printf("Failed to marshal event: %+v\n", event)
				fmt.Printf("%s\n", err)
				return
			}

			_, err = logFile.WriteString(string(eventJson) + "\n")
			if err != nil {
				fmt.Printf("Failed to write to event file: %s\n", err)
				return
			}
		}
	}(watcher, logFile)

	return watcher, nil
}
