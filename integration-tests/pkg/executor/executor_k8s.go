package executor

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/log"
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

func NewK8sExecutor() (*K8sExecutor, error) {
	log.Info("Creating k8s configuration")
	config, err := rest.InClusterConfig()
	if err != nil {
		return nil, log.ErrorWrap(err, "Failed to get cluster config")
	}

	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		return nil, log.ErrorWrap(err, "Failed to create client")
	}

	k8s := &K8sExecutor{
		clientset: clientset,
	}
	return k8s, nil
}

func (e *K8sExecutor) IsPodRunning(podName string) (bool, error) {
	pod, err := e.clientset.CoreV1().Pods(TESTS_NAMESPACE).Get(context.Background(), podName, metaV1.GetOptions{})
	if err != nil {
		return false, err
	}

	if pod == nil || pod.Status.Phase != coreV1.PodRunning {
		return false, nil
	}

	return pod.Status.ContainerStatuses[0].Ready, nil
}

func (e *K8sExecutor) PodContainerID(podFilter ContainerFilter) string {
	pod, err := e.ClientSet().CoreV1().Pods(podFilter.Namespace).Get(context.Background(), podFilter.Name, metaV1.GetOptions{})
	if err != nil {
		log.Error("namespace: %s pod: %s error: %s\n", podFilter.Namespace, podFilter.Name, err)
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
		log.Error("Invalid container ID: %q", containerID)
		return ""
	}

	i := strings.LastIndex(containerID, "/")
	if i == -1 {
		log.Error("Invalid container ID: %q", containerID)
		return ""
	}

	return common.ContainerShortID(containerID[i+1:])
}

func (e *K8sExecutor) PodExists(podFilter ContainerFilter) (bool, error) {
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

func (e *K8sExecutor) RemovePod(podFilter ContainerFilter) (string, error) {
	err := e.clientset.CoreV1().Pods(podFilter.Namespace).Delete(context.Background(), podFilter.Name, metaV1.DeleteOptions{})
	return "", err
}

func (e *K8sExecutor) WaitPodRemoved(podFilter ContainerFilter) error {
	ctx := context.Background()
	opts := metaV1.ListOptions{LabelSelector: "app=" + podFilter.Name}
	w, err := e.clientset.CoreV1().Pods(podFilter.Namespace).Watch(ctx, opts)
	if err != nil {
		return err
	}
	defer w.Stop()

	timer := time.After(1 * time.Minute)

	for {
		select {
		case event := <-w.ResultChan():
			if event.Type == watch.Deleted {
				return nil
			}
		case <-ctx.Done():
			return nil
		case <-timer:
			return fmt.Errorf("Timed out waiting for pod to be deleted: %+v", podFilter)
		}
	}
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
				log.Error("Failed to marshal event %+v: %s", event, err)
				return
			}

			_, err = logFile.WriteString(string(eventJson) + "\n")
			if err != nil {
				log.Error("Failed to write to event file: %s", err)
				return
			}
		}
	}(watcher, logFile)

	return watcher, nil
}

func (e *K8sExecutor) CreateConfigMap(ns string, configMap *coreV1.ConfigMap) (*coreV1.ConfigMap, error) {
	return e.ClientSet().CoreV1().ConfigMaps(ns).Create(context.Background(), configMap, metaV1.CreateOptions{})
}

func (e *K8sExecutor) UpdateConfigMap(ns string, configMap *coreV1.ConfigMap) (*coreV1.ConfigMap, error) {
	return e.ClientSet().CoreV1().ConfigMaps(ns).Update(context.Background(), configMap, metaV1.UpdateOptions{})
}

func (e *K8sExecutor) RemoveConfigMap(ns, name string) error {
	return e.ClientSet().CoreV1().ConfigMaps(ns).Delete(context.Background(), name, metaV1.DeleteOptions{})
}
