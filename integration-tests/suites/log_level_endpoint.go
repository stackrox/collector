package suites

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/log"
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
	err := checkLogLevel("DEBUG")
	s.Assert().NoError(err)

	level := bytes.NewBuffer([]byte("info"))
	resp, err := http.Post(collectorUrl, "application/text", level)
	s.Assert().NoError(err)
	s.Assert().Equal(resp.StatusCode, 200)

	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	s.Assert().NoError(err)
	s.Assert().True(json.Valid(body))

	err = checkLogLevel("INFO")
	s.Assert().NoError(err)
}

func checkLogLevel(level string) error {
	valid, err := http.Post(collectorUrl, "application/text", nil)
	if err != nil {
		return err
	}
	if valid.StatusCode != 200 {
		return log.Error("Got status code %s", valid.Status)
	}

	defer valid.Body.Close()
	bodyV, err := io.ReadAll(valid.Body)
	if err != nil {
		return err
	}

	var logLevel struct {
		Status string
		Level  string
	}

	err = json.Unmarshal(bodyV, &logLevel)
	if err != nil {
		return err
	}

	if level != logLevel.Level {
		return log.Error("Invalid log level, got=%q want=%q", logLevel.Level, level)
	}
	return nil
}
