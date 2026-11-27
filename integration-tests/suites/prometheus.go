package suites

import (
	"net/http"

	"github.com/prometheus/common/expfmt"
	"github.com/prometheus/common/model"
)

type PrometheusTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *PrometheusTestSuite) SetupSuite() {
	s.StartCollector(false, nil)
}

func (s *PrometheusTestSuite) TearDownSuite() {
	s.StopCollector()
}

// Verify prometheus metrics are being exported
func (s *PrometheusTestSuite) TestPrometheus() {
	response, err := http.Get("http://localhost:9090/metrics")
	s.Require().NoError(err)
	s.Assert().Equal(response.StatusCode, 200)

	// Parse the body to check it is a valid prometheus output
	body := response.Body
	defer body.Close()
	parser := expfmt.NewTextParser(model.LegacyValidation)
	m, err := parser.TextToMetricFamilies(body)
	s.Require().NoError(err)
	s.Assert().NotEmpty(m)
}
