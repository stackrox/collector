//go:build k8s
// +build k8s

package integrationtests

import (
	"testing"

	"github.com/stretchr/testify/suite"

	suites "github.com/stackrox/collector/integration-tests/suites/k8s"
)

func TestK8sNamespace(t *testing.T) {
	t.Skip("Skipping test")
	if testing.Short() {
		t.Skip("Not running k8s in short mode")
	}
	suite.Run(t, new(suites.K8sNamespaceTestSuite))
}

func TestK8sConfigReload(t *testing.T) {
	if testing.Short() {
		t.Skip("Not running k8s in short mode")
	}
	suite.Run(t, new(suites.K8sConfigReloadTestSuite))
}
