package assert

import (
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"time"

	"github.com/davecgh/go-spew/spew"
	"github.com/google/go-cmp/cmp"
	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

var (
	runtimeConfigErrorMsg = "Runtime configuration was not updated"

	tickTime = 1 * time.Second
	timeout  = 3 * time.Minute
)

func getCollectorRuntimeConfig(t *testing.T, collectorIP string) types.RuntimeConfig {
	body, err := collector.IntrospectionQuery(collectorIP, "/state/runtime-config")
	assert.NoError(t, err)
	var response types.RuntimeConfig
	err = json.Unmarshal(body, &response)
	assert.NoError(t, err)
	return response
}

func AssertRuntimeConfig(t *testing.T, collectorIP string, config types.RuntimeConfig) {
	AssertRepeated(t, tickTime, timeout, runtimeConfigErrorMsg, func() bool {
		collectorConfig := getCollectorRuntimeConfig(t, collectorIP)
		return cmp.Equal(config, collectorConfig)
	})
}

func AssertExternalIps(t *testing.T, enabled string, collectorIP string) {
	AssertRepeated(t, tickTime, timeout, runtimeConfigErrorMsg, func() bool {
		collectorConfig := getCollectorRuntimeConfig(t, collectorIP)
		return collectorConfig.Networking.ExternalIps.Enabled == enabled
	})
}

func AssertNoRuntimeConfig(t *testing.T, collectorIP string) {
	AssertRepeated(t, tickTime, timeout, runtimeConfigErrorMsg, func() bool {
		body, err := collector.IntrospectionQuery(collectorIP, "/state/runtime-config")
		assert.NoError(t, err)
		return strings.TrimSpace(string(body)) == "{}"
	})
}

func AssertRepeated(t *testing.T, tickTime time.Duration, timeout time.Duration, msg string, condition func() bool) {
	tick := time.NewTicker(tickTime)
	timer := time.After(timeout)

	for {
		select {
		case <-tick.C:
			if condition() {
				// Condition has been met
				return
			}

		case <-timer:
			log.Error("Timeout reached: " + msg)
			t.FailNow()
		}
	}
}

func ElementsMatchFunc[N any](expected []N, actual []N, equal func(a, b N) bool) bool {
	if len(expected) != len(actual) {
		return false
	}

	for i := range expected {
		if !equal(expected[i], actual[i]) {
			return false
		}
	}
	return true
}

func ListsToAssertMsg[N any](expected []N, actual []N) string {
	return fmt.Sprintf(
		"Expected elements:\n%s\n\nActual elements:\n%s\n",
		spew.Sdump(expected),
		spew.Sdump(actual),
	)
}

func AssertElementsMatchFunc[N any](t *testing.T, expected []N, actual []N, equal func(a, b N) bool) bool {
	match := ElementsMatchFunc(expected, actual, equal)
	if !match {
		assertMsg := ListsToAssertMsg(expected, actual)
		assert.Fail(t, assertMsg)
	}
	return match
}
