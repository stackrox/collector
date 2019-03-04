package integrationtests

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"time"
	"io/ioutil"
	"testing"
	"encoding/json"

	"github.com/boltdb/bolt"
	"github.com/stretchr/testify/assert"
)

const (
	processBucket = "Process"
	networkBucket = "Network"
)

type testConfig struct {
	dbpath          string
	db              *bolt.DB
	serverIP        string
	serverPort      string
	clientIP        string
	clientPort      string
	serverContainer string
	clientContainer string
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func setup(s *testConfig, t *testing.T) {
	dockerComposeUp(t)

	// invokes default nginx
	containerID, err := launchContainer("nginx", "nginx:1.14-alpine", "")
	assert.Nil(t, err)
	s.serverContainer = containerID[0:12]

	// invokes "sleep"
	_, err = execContainer("nginx", []string{"sh", "-c", "sleep 5"})
	assert.Nil(t, err)

	// invokes another container
	containerID, err = launchContainer("nginx-curl", "ewoutp/docker-nginx-curl", "")
	assert.Nil(t, err)
	s.clientContainer = containerID[0:12]

	ip, err := getIPAddress("nginx")
	assert.Nil(t, err)
	s.serverIP = ip

	port, err := getPort("nginx")
	assert.Nil(t, err)
	s.serverPort = port

	_, err = execContainer("nginx-curl", []string{"curl", ip})
	assert.Nil(t, err)

	ip, err = getIPAddress("nginx-curl")
	assert.Nil(t, err)
	s.clientIP = ip

	port, err = getPort("nginx-curl")
	assert.Nil(t, err)
	s.clientPort = port

	time.Sleep(20 * time.Second)

	logs, err := containerLogs("test_collector_1")
	assert.NoError(t, err)
	err = ioutil.WriteFile("collector.logs", []byte(logs), 0644)
	assert.NoError(t, err)

	logs, err = containerLogs("test_grpc-server_1")
	err = ioutil.WriteFile("grpc_server.logs", []byte(logs), 0644)
	assert.NoError(t, err)

	// bring down server
	dockerComposeDown(t)
	// sleep for few
	time.Sleep(2 * time.Second)

	s.db, err = BoltDB(t)
	if err != nil {
		assert.FailNow(t, "DB file could not be opened", "Error: %s", err)
	}
}

func TestProcessViz(t *testing.T) {
	s := &testConfig{}
	setup(s, t)

	processName := "nginx"
	exeFilePath := "/usr/local/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := Get(s.db, processName, processBucket)
	assert.Nil(t, err)
	assert.Equal(t, expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = Get(s.db, processName, processBucket)
	assert.Nil(t, err)
	assert.Equal(t, expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = Get(s.db, processName, processBucket)
	assert.Nil(t, err)
	assert.Equal(t, expectedProcessInfo, val)

	cleanup(t)
}

func TestNetworkFlows(t *testing.T) {
	s := &testConfig{}
	setup(s, t)

	// Server side checks
	val, err := Get(s.db, s.serverContainer, networkBucket)
	assert.Nil(t, err)
	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	actualValues := strings.Split(string(val), ":")
	assert.Len(t, actualValues, 6)
	expectedServerIP := actualValues[0]
	expectedServerPort := actualValues[1]
	expectedClientIP := actualValues[2]
	// client port are chosen at random so not checking that

	assert.Equal(t, expectedServerIP, expectedServerIP)
	assert.Equal(t, expectedServerPort, expectedServerPort)
	assert.Equal(t, expectedClientIP, expectedClientIP)
	assert.Equal(t, "ROLE_SERVER", actualValues[5])

	// client side checks
	val, err = Get(s.db, s.clientContainer, networkBucket)
	assert.Nil(t, err)
	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s, Port: %s\n", s.clientContainer, s.clientIP, s.clientPort)

	actualValues = strings.Split(string(val), ":")
	assert.Len(t, actualValues, 6)
	expectedClientIP = actualValues[0]
	expectedServerIP = actualValues[2]
	expectedServerPort = actualValues[3]
	assert.Equal(t, "ROLE_CLIENT", actualValues[5])

	assert.Equal(t, expectedServerIP, expectedServerIP)
	assert.Equal(t, expectedServerPort, expectedServerPort)
	assert.Equal(t, expectedClientIP, expectedClientIP)

	cleanup(t)
}

func cleanup(t *testing.T) {
	dockerComposeDown(t)
	cleanupContainer([]string{"nginx", "nginx-curl"})
}

func launchContainer(containerName, imageName, command string) (string, error) {
	var cmd *exec.Cmd
	if command != "" {
		cmd = exec.Command("docker", "run", "-d", "--name", containerName, imageName, "/bin/sleep 300")
	} else {
		cmd = exec.Command("docker", "run", "-d", "--name", containerName, imageName)
	}
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func execContainer(containerName string, command []string) (string, error) {
	args := []string{"exec", containerName}
	args = append(args, command...)
	cmd := exec.Command("docker", args...)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func getIPAddress(containerName string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(strings.Replace(string(stdoutStderr), "'", "", -1), "\n"), err
}

func getPort(containerName string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format={{json .NetworkSettings.Ports}}", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	if err != nil {
		return "", err
	}
	rawString := strings.Trim(string(stdoutStderr), "\n")
	var portMap map[string]interface{}
	err = json.Unmarshal([]byte(rawString), &portMap)
	if err != nil {
		return "", err
	}

	for k := range portMap {
		return strings.Split(k, "/")[0], nil
	}

	return "", fmt.Errorf("no port mapping found: %v %v", rawString, portMap)
}

func cleanupContainer(containers []string) {
	for _, container := range containers {
		exec.Command("docker", "kill", container).Run()
		exec.Command("docker", "rm", container).Run()
	}
}

func dockerComposeUp(t *testing.T) error {
	err := dockerCompose("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "up", "-d")
	if err != nil {
		t.Fatal("Unable to deploy the stack", err.Error())
	}

	time.Sleep(10 * time.Second)
	return err
}

func containerLogs(containerName string) (string, error) {
	cmd := exec.Command("docker", "logs", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func dockerComposeDown(t *testing.T) error {
	err := dockerCompose("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "down", "--volumes")
	if err != nil {
		t.Fatalf("Unable to tear down the stack", err.Error())
	}

	return err
}

func dockerComposePorts(serviceName, containerPort string) (*bytes.Buffer, error) {
	return dockerComposeStdout("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "port", serviceName, containerPort)
}

func dockerCompose(name string, arg ...string) error {
	dockerCompose := exec.Command(name, arg...)
	dockerCompose.Stdout = os.Stdout
	dockerCompose.Stderr = os.Stderr

	err := dockerCompose.Run()

	return err
}

func dockerComposeStdout(name string, arg ...string) (*bytes.Buffer, error) {
	var stdout bytes.Buffer

	dockerCompose := exec.Command(name, arg...)
	dockerCompose.Stdout = &stdout
	dockerCompose.Stderr = os.Stderr

	err := dockerCompose.Run()

	return &stdout, err
}

func BoltDB(t *testing.T) (db *bolt.DB, err error) {
	dbpath := "/tmp/collector-test.db"
	opts := &bolt.Options{ReadOnly: true}
	db, err = bolt.Open(dbpath, 0600, opts)
	assert.Nil(t, err)
	return
}

func Get(db *bolt.DB, key string, bucket string) (val string, err error) {
	if db == nil {
		return "", fmt.Errorf("Db %v is nil", db)
	}
	err = db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))
		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}
		val = string(b.Get([]byte(key)))
		return nil
	})
	return
}
