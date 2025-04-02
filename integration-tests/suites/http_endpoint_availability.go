package suites

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/log"
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
	collectorUrl, err := url.Parse(fmt.Sprintf("http://localhost:%d", s.Port))
	s.Assert().NoError(err)
	for _, endpoint := range s.Endpoints {
		endpointUrl := collectorUrl.JoinPath(endpoint)

		log.Info("url: %s", endpointUrl)

		resp, err := http.Get(endpointUrl.String())
		s.Require().NoError(err)
		s.Require().NotNil(resp)
		s.Require().Equal(resp.StatusCode, http.StatusOK, "HTTP status code")

		var buf bytes.Buffer
		defer resp.Body.Close()
		_, err = buf.ReadFrom(resp.Body)
		s.Require().NoError(err)
		s.Require().True(json.Valid(buf.Bytes()))
	}
}

func (s *HttpEndpointAvailabilityTestSuite) TearDownSuite() {
	s.StopCollector()
}
