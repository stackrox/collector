package executor

import (
	"context"
	"fmt"
	"strings"

	coreV1 "k8s.io/api/core/v1"
	metaV1 "k8s.io/apimachinery/pkg/apis/meta/v1"
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

type PodFilter struct {
	Name      string
	Namespace string
}

func (e *K8sExecutor) ContainerID(podFilter interface{}) string {
	pf, ok := podFilter.(PodFilter)
	if !ok {
		return ""
	}

	pod, err := e.ClientSet().CoreV1().Pods(pf.Namespace).Get(context.Background(), pf.Name, metaV1.GetOptions{})
	if err != nil {
		fmt.Printf("%s\n", err)
		return ""
	}

	if len(pod.Status.ContainerStatuses) != 1 {
		return ""
	}

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

	return containerID[i+1 : i+13]
}

func (e *K8sExecutor) ContainerExists(podFilter interface{}) (bool, error) {
	pf, ok := podFilter.(PodFilter)
	if !ok {
		return false, fmt.Errorf("Wrong filter type. Expected=PodFilter, got=%T", podFilter)
	}

	pod, err := e.clientset.CoreV1().Pods(pf.Namespace).Get(context.Background(), pf.Name, metaV1.GetOptions{})
	if err != nil {
		return false, err
	}

	return pod != nil, nil
}

func (e *K8sExecutor) ExitCode(podFilter interface{}) (int, error) {
	pf, ok := podFilter.(PodFilter)
	if !ok {
		return -1, fmt.Errorf("Wrong filter type. Expected=PodFilter, got=%T", podFilter)
	}

	pod, err := e.clientset.CoreV1().Pods(pf.Namespace).Get(context.Background(), pf.Name, metaV1.GetOptions{})
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

func (e *K8sExecutor) RemoveContainer(podFilter interface{}) (string, error) {
	pf, ok := podFilter.(PodFilter)
	if !ok {
		return "", fmt.Errorf("Wrong pod filter type. Expected=PodFilter, got=%T", podFilter)
	}

	err := e.clientset.CoreV1().Pods(pf.Namespace).Delete(context.Background(), pf.Name, metaV1.DeleteOptions{})
	return "", err
}

func (e *K8sExecutor) StopContainer(name string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *K8sExecutor) CreateNamespace(ns string) (*coreV1.Namespace, error) {
	meta := metaV1.ObjectMeta{Name: ns}
	return e.clientset.CoreV1().Namespaces().Create(context.Background(), &coreV1.Namespace{ObjectMeta: meta}, metaV1.CreateOptions{})
}

func (e *K8sExecutor) CreatePod(ns string, pod *coreV1.Pod) (*coreV1.Pod, error) {
	return e.clientset.CoreV1().Pods(ns).Create(context.Background(), pod, metaV1.CreateOptions{})
}

func (e *K8sExecutor) ClientSet() *kubernetes.Clientset {
	return e.clientset
}
