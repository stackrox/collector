package suites

import (
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
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
	s.runProfilerTest("heap")
}

func (s *GperftoolsTestSuite) TestFetchCpuProfile() {
	s.runProfilerTest("cpu")
}

func (s *GperftoolsTestSuite) runProfilerTest(resource string) {
	heap_api_url := fmt.Sprintf("http://localhost:8080/profile/%s", resource)
	data_type := "application/x-www-form-urlencoded"

	response, err := http.Post(heap_api_url, data_type, strings.NewReader("on"))
	s.Require().NoError(err)
	s.Assert().Equalf(response.StatusCode, 200, "Failed to start %s profiling", resource)

	// Wait a bit to collect something in the heap profile
	common.Sleep(1 * time.Second)

	response, err = http.Post(heap_api_url, data_type, strings.NewReader("off"))
	s.Require().NoError(err)
	s.Assert().Equalf(response.StatusCode, 200, "Failed to stop %s profiling", resource)

	response, err = http.Get(heap_api_url)
	s.Require().NoError(err)
	s.Assert().Equalf(response.StatusCode, 200, "Failed to fetch %s profile", resource)
}
