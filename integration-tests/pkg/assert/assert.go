package assert

import (
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"time"

	"github.com/davecgh/go-spew/spew"
	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

var (
	runtimeConfigErrorMsg = "Runtime configuration was not updated"
)

func AssertExternalIps(t *testing.T, enabled bool, collectorIP string) {
	tickTime := 1 * time.Second
	timeout := 3 * time.Minute
	AssertRepeated(t, tickTime, timeout, runtimeConfigErrorMsg, func() bool {
		body, err := collector.IntrospectionQuery(collectorIP, "/state/config")
		assert.NoError(t, err)
		var response types.RuntimeConfig
		err = json.Unmarshal(body, &response)
		assert.NoError(t, err)

		return response.Networking.ExternalIps.Enable == enabled
	})
}

func AssertNoRuntimeConfig(t *testing.T, collectorIP string) {
	tickTime := 1 * time.Second
	timeout := 3 * time.Minute
	AssertRepeated(t, tickTime, timeout, runtimeConfigErrorMsg, func() bool {
		body, err := collector.IntrospectionQuery(collectorIP, "/state/config")
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
