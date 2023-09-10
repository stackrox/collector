//go:build bench
// +build bench

package integrationtests

import (
	"testing"

	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites"
)

func TestBenchmarkBaseline(t *testing.T) {
	if testing.Short() {
		t.Skip("Not running Benchmarks in short mode")
	}
	suite.Run(t, new(suites.BenchmarkBaselineTestSuite))
}

func TestBenchmarkCollector(t *testing.T) {
	if testing.Short() {
		t.Skip("Not running Benchmarks in short mode")
	}
	suite.Run(t, new(suites.BenchmarkCollectorTestSuite))
}
