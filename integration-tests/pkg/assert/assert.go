package assert

import (
	"encoding/json"
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
)

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
	formatList := func(list []N) string {
		data, err := json.MarshalIndent(list, "", "  ")
		if err != nil {
			return fmt.Sprintf("Failed to format list: %v", err)
		}
		return string(data)
	}

	return fmt.Sprintf(
		"Expected elements:\n%s\n\nActual elements:\n%s\n",
		formatList(expected),
		formatList(actual),
	)
}

func AssertElementsMatchFunc[N any](t *testing.T, expected []N, actual []N, equal func(a, b N) bool) bool {
	match := ElementsMatchFunc(expected, actual, equal)
	if !match {
		assertMsg := ListsToAssertMsg(expected, actual)
		assert.True(t, match, assertMsg)
	}
	return match
}
