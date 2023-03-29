package integrationtests

import (
	"testing"

	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites"
)

func TestManyProcessesListeningOnPorts(t *testing.T) {
	manyProcessesListeningOnPortsTestSuite := &suites.ManyProcessesListeningOnPortsTestSuite{
		NumPorts: 1000,
	}
	suite.Run(t, manyProcessesListeningOnPortsTestSuite)
}
