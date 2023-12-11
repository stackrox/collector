package suites

import (
	"net/http"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
)

type GperftoolsTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *GperftoolsTestSuite) SetupSuite() {
	if ok, _ := common.ArchSupported("x86_64"); !ok {
		return
	}

	s.StartCollector(false, nil)
}

func (s *GperftoolsTestSuite) TearDownSuite() {
	if ok, _ := common.ArchSupported("x86_64"); !ok {
		return
	}

	s.StopCollector()
}

// Verify Gperftools API:
// * turn heap profiling on
// * turn heap profiling off
// * fetch the profile
//
// NOTE: The test will only be performed on supported architectures (only
// x86_64 at the moment).
func (s *GperftoolsTestSuite) TestFetchHeapProfile() {
	if ok, arch := common.ArchSupported("x86_64"); !ok {
		s.T().Skip("[WARNING]: skip GperftoolsTestSuite on ", arch)
	}

	var (
		response *http.Response
		err      error
	)

	heap_api_url := "http://localhost:8080/profile/heap"
	data_type := "application/x-www-form-urlencoded"

	response, err = http.Post(heap_api_url, data_type, strings.NewReader("on"))
	s.Require().NoError(err)
	s.Assert().Equal(response.StatusCode, 200, "Failed to start heap profiling")

	// Wait a bit to collect something in the heap profile
	time.Sleep(1 * time.Second)

	response, err = http.Post(heap_api_url, data_type, strings.NewReader("off"))
	s.Require().NoError(err)
	s.Assert().Equal(response.StatusCode, 200, "Failed to stop heap profiling")

	response, err = http.Get(heap_api_url)
	s.Require().NoError(err)
	s.Assert().Equal(response.StatusCode, 200, "Failed to fetch heap profile")
}
