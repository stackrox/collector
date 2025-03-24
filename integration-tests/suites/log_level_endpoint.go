package suites

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
)

const collectorUrl = "http://localhost:8080/loglevel"

type LogLevelTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *LogLevelTestSuite) SetupSuite() {
	options := &collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_INTROSPECTION_ENABLE": "true",
		},
	}
	s.StartCollector(false, options)
}

func (s *LogLevelTestSuite) TearDownSuite() {
	s.StopCollector()
}

func (s *LogLevelTestSuite) TestLogLevel() {
	level := []byte("info")
	resp, err := http.Post(collectorUrl, "application/text", bytes.NewBuffer(level))
	s.Assert().NoError(err)
	s.Assert().Equal(resp.StatusCode, 200)

	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	s.Assert().NoError(err)
	s.Assert().True(json.Valid(body))

	valid, err := http.Post(collectorUrl, "application/text", nil)
	s.Assert().NoError(err)
	s.Assert().Equal(valid.StatusCode, 200)

	defer valid.Body.Close()
	bodyV, err := io.ReadAll(valid.Body)
	s.Assert().NoError(err)

	var logLevel struct {
		Status string
		Level  string
	}

	err = json.Unmarshal(bodyV, &logLevel)
	s.Assert().NoError(err)
	s.Assert().Equal("INFO", logLevel.Level)
}
