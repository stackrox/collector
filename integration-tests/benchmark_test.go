//go:build all
// +build all

package integrationtests

import (
	"testing"

	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites"
)

func TestBenchmarkBaseline(t *testing.T) {
	suite.Run(t, new(suites.BenchmarkBaselineTestSuite))
}

func TestBenchmarkCollector(t *testing.T) {
	suite.Run(t, new(suites.BenchmarkCollectorTestSuite))
}
