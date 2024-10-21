package introspection_endpoints

import (
	"testing"

	"bytes"
	"encoding/json"
	"net/http"
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/pkg/types"
)

func GetRuntimeConfig(t *testing.T) types.RuntimeConfig {
	url := "http://localhost:8080/state/config"

	resp, err := http.Get(url)

	assert.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.StatusCode, http.StatusOK, "HTTP status code")

	defer resp.Body.Close()
	var buf bytes.Buffer
	_, err = buf.ReadFrom(resp.Body)
	assert.NoError(t, err)
	var runtimeConfig types.RuntimeConfig
	err = json.Unmarshal(buf.Bytes(), &runtimeConfig)
	assert.NoError(t, err)

	return runtimeConfig
}

func ExpectRuntimeConfig(t *testing.T, timeout time.Duration, expected types.RuntimeConfig) bool {
	timer := time.NewTimer(timeout)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-timer.C:
			t.Log("timed out waiting for the expected runtime config")
			return false
		case <-ticker.C:
			runtimeConfig := GetRuntimeConfig(t)
			if runtimeConfig == expected {
				return true
			}
		}
	}
}
