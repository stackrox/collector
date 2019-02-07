package integrationtests

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/suite"
	"github.com/stretchr/testify/assert"
	"github.com/boltdb/bolt"
)

const (
	processBucket = "Process"
)

func TestCollectorGRPC(t *testing.T) {
	suite.Run(t, new(IntegrationTestSuite))
}

type IntegrationTestSuite struct {
	suite.Suite
	dbpath string
	db *bolt.DB
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *IntegrationTestSuite) SetupSuite() {
	s.dockerComposeUp()
	s.dbpath = "/tmp/collector-test.db"

	// invokes default nginx
	_, err := s.launchContainer()
	assert.Nil(s.T(), err)

	// invokes "sh"
	_, err = s.execContainer()
	assert.Nil(s.T(), err)

	// bring down server
	s.dockerComposeDown()
	// sleep for few
	time.Sleep(2 * time.Second)

	s.db, err = s.BoltDB()
	if err != nil {
		assert.FailNow(s.T(), "DB file could not be opened", "Error: %s", err)
	}
}

func (s *IntegrationTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/sbin/nginx"
	val, err := s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), exeFilePath, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	val, err = s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), exeFilePath, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	val, err = s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), exeFilePath, val)
}

func (s *IntegrationTestSuite) TearDownSuite() {
	s.dockerComposeDown()
	s.cleanupContainer()
}

func (s *IntegrationTestSuite) launchContainer() (string, error) {
	cmd := exec.Command("docker", "run", "-d", "--name", "cowabunga", "nginx:1.14-alpine")
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) execContainer() (string, error) {
	cmd := exec.Command("docker", "exec", "cowabunga", "sh", "-c", "sleep 5")
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) cleanupContainer() {
	exec.Command("docker", "kill", "cowabunga").Run()
//	exec.Command("docker", "rm", "cowabunga").Run()
}

func (s *IntegrationTestSuite) dockerComposeUp() error {
	err := s.dockerCompose("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "up", "-d")
	if err != nil {
		s.T().Fatal("Unable to deploy the stack", err.Error())
	}

	time.Sleep(10 * time.Second)

	return err
}

func (s *IntegrationTestSuite) dockerComposeDown() error {
	err := s.dockerCompose("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "down", "--volumes")
	if err != nil {
		s.T().Fatal("Unable to tear down the stack", err.Error())
	}

	return err
}

func (s *IntegrationTestSuite) dockerComposePorts(serviceName, containerPort string) (*bytes.Buffer, error) {
	return s.dockerComposeStdout("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "port", serviceName, containerPort)
}

func (s *IntegrationTestSuite) dockerCompose(name string, arg ...string) error {
	dockerCompose := exec.Command(name, arg...)
	dockerCompose.Stdout = os.Stdout
	dockerCompose.Stderr = os.Stderr

	err := dockerCompose.Run()

	return err
}

func (s *IntegrationTestSuite) dockerComposeStdout(name string, arg ...string) (*bytes.Buffer, error) {
	var stdout bytes.Buffer

	dockerCompose := exec.Command(name, arg...)
	dockerCompose.Stdout = &stdout
	dockerCompose.Stderr = os.Stderr

	err := dockerCompose.Run()

	return &stdout, err
}

func (s *IntegrationTestSuite) BoltDB() (db *bolt.DB, err error) {
	opts := &bolt.Options{ReadOnly: true}
	db, err = bolt.Open(s.dbpath, 0600, opts)
	if err != nil  {
		fmt.Printf("Permission error. %v\n", err)
	}
	return db, err
}

func (s *IntegrationTestSuite) Get(key string, bucket string) (val string, err error) {
	if s.db == nil {
		return "", fmt.Errorf("Db %v is nil", s.db)
	}
	err = s.db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))
		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}
		val = string(b.Get([]byte(key)))
		return nil
	})
	return
}
