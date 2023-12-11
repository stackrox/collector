package suites

import (
	"bytes"
	"fmt"
	"net/http"
	"strings"
	"time"

	"golang.org/x/sys/unix"
)

type GperftoolsTestSuite struct {
	IntegrationTestSuiteBase
}

// IsArchSupported: at the moment only x86_64 builds have gperftools
func (s *GperftoolsTestSuite) IsArchSupported() (bool, string) {
	u := unix.Utsname{}
	unix.Uname(&u)

	// Exclude null bytes from comparison
	arch := string(bytes.Trim(u.Machine[:], "\x00"))
	return arch == "x86_64", arch
}

func (s *GperftoolsTestSuite) SetupSuite() {
	if ok, _ := s.IsArchSupported(); !ok {
		return
	}

	s.StartCollector(false, nil)
}

func (s *GperftoolsTestSuite) TearDownSuite() {
	if ok, _ := s.IsArchSupported(); !ok {
		return
	}

	s.StopCollector()
}

// Verify Gperftools API:
// * turn heap profiling on
// * turn heap profiling off
// * fetch the profile
//
// NOTE: The test will only be performed on supported architectures, as defined
// by IsArchSupported.
func (s *GperftoolsTestSuite) TestFetchHeapProfile() {
	heap_api_url := "http://localhost:8080/profile/heap"
	data_type := "application/x-www-form-urlencoded"

	if ok, arch := s.IsArchSupported(); !ok {
		fmt.Printf("[WARNING]: Skip GperftoolsTestSuite on %s\n", arch)
		return
	}

	var (
		response *http.Response
		err      error
	)

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
