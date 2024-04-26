package suites

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
)

type HttpEndpointAvailabilityTestSuite struct {
	IntegrationTestSuiteBase
	Port             int
	CollectorOptions collector.StartupOptions
	Endpoints        []string
}

func (s *HttpEndpointAvailabilityTestSuite) SetupSuite() {
	s.StartCollector(false, &s.CollectorOptions)
}

func (s *HttpEndpointAvailabilityTestSuite) TestAvailability() {
	for _, endpoint := range s.Endpoints {
		url := fmt.Sprintf("http://localhost:%d%s", s.Port, endpoint)

		fmt.Println(url)

		resp, err := http.Get(url)
		s.Require().NoError(err)
		s.Require().NotNil(resp)
		if resp != nil {
			s.Require().Equal(resp.StatusCode, http.StatusOK, "HTTP status code")

			var buf bytes.Buffer
			defer resp.Body.Close()
			_, err = buf.ReadFrom(resp.Body)
			s.Require().NoError(err)
			s.Require().True(json.Valid(buf.Bytes()))
		}

	}
}

func (s *HttpEndpointAvailabilityTestSuite) TearDownSuite() {
	s.StopCollector()
}
