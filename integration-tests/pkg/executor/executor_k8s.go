package executor

import (
	"context"
	"fmt"

	coreV1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
)

const (
	TESTS_NAMESPACE = "collector-tests"
)

type k8sExecutor struct {
	clientset *kubernetes.Clientset
}

func NewK8sExecutor() *k8sExecutor {
	config, err := rest.InClusterConfig()
	if err != nil {
		fmt.Printf("Error: Failed to get cluster config: %s\n", err)
		return nil
	}

	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		fmt.Printf("Error: Failed to create client: %s", err)
		return nil
	}

	return &k8sExecutor{
		clientset: clientset,
	}
}

func (e *k8sExecutor) CopyFromHost(src string, dst string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) PullImage(image string) error {
	return fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) IsContainerRunning(podName string) (bool, error) {
	pod, err := e.clientset.CoreV1().Pods(TESTS_NAMESPACE).Get(context.Background(), podName, metav1.GetOptions{})
	if err != nil {
		return false, err
	}

	if pod == nil || pod.Status.Phase != coreV1.PodRunning {
		return false, nil
	}

	return pod.Status.ContainerStatuses[0].Ready, nil
}

func (e *k8sExecutor) ContainerExists(podName string) (bool, error) {
	pod, err := e.clientset.CoreV1().Pods(TESTS_NAMESPACE).Get(context.Background(), podName, metav1.GetOptions{})
	if err != nil {
		return false, err
	}

	return pod != nil, nil
}

func (e *k8sExecutor) ExitCode(podName string) (int, error) {
	pod, err := e.clientset.CoreV1().Pods(TESTS_NAMESPACE).Get(context.Background(), podName, metav1.GetOptions{})
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

func (e *k8sExecutor) Exec(args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) ExecWithStdin(pipedContent string, args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) ExecWithoutRetry(args ...string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) KillContainer(name string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) RemoveContainer(name string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}

func (e *k8sExecutor) StopContainer(name string) (string, error) {
	return "", fmt.Errorf("Unimplemented")
}
