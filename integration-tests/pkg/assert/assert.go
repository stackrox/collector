package assert

import (
	"bytes"
	"encoding/json"
	"fmt"
	"testing"

	"github.com/davecgh/go-spew/spew"
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
	var expectedBuf, actualBuf bytes.Buffer
	spew.Fdump(&expectedBuf, expected)
	spew.Fdump(&actualBuf, actual)
	return fmt.Sprintf(
		"Expected elements:\n%s\n\nActual elements:\n%s\n",
		expectedBuf.String(),
		actualBuf.String(),
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
