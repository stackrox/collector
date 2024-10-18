package mock_sensor

import (
	"testing"

	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/stretchr/testify/assert"
	//"github.com/thoas/go-funk"

	"github.com/stackrox/collector/integration-tests/pkg/types"
	//"github.com/stackrox/collector/integration-tests/pkg/log"
)

func GetRuntimeConfig(t *testing.T) types.RuntimeConfig {
	url := "http://localhost:8080/state/config"

	resp, err := http.Get(url)

	//fmt.Println(resp)

	assert.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.StatusCode, http.StatusOK, "HTTP status code")

	//defer resp.Body.Close()

	//body, err := ioutil.ReadAll(resp.Body)
	assert.NoError(t, err)

	//fmt.Println(body)

	defer resp.Body.Close()
	var buf bytes.Buffer
	_, err = buf.ReadFrom(resp.Body)
	assert.NoError(t, err)
	jsonString := buf.String()
	fmt.Println("Raw JSON:", jsonString)
	//assert.True(json.Valid(buf.Bytes()))
	var runtimeConfig types.RuntimeConfig
	err = json.Unmarshal(buf.Bytes(), &runtimeConfig)
	assert.NoError(t, err)

	return runtimeConfig
}

func (s *MockSensor) ExpectRuntimeConfig(t *testing.T, timeout time.Duration, expected types.RuntimeConfig) bool {
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
			//if assert.Equal(t, expected, runtimeConfig, "Checking runtime config") {
			if runtimeConfig == expected {
				return true
			}
		}
	}
}

//func (s *MockSensor) ExpectRuntimeConfig(t *testing.T, timeout time.Duration, expected types.RuntimeConfig) bool {
//	timer := time.After(timeout)
//
//	for {
//		select {
//		case <-timer:
//			return assert.Equal(t, expected, GetRuntimeConfig(t), "timed out waiting for networks")
//		case runtimeConfig := GetRuntimeConfig(t):
//			if runtimeConfig == expected {
//				return true
//			}
//		}
//	}
//}
