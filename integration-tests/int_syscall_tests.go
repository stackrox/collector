package integrationtests

import (
	"testing"
	"github.com/stretchr/testify/suite"
)

var (
	syscalls = []string {
      "accept",
      "chdir",
      "clone",
      "close",
      "connect",
      "execve",
      "fchdir",
      "fork",
      "procexit",
      "procinfo",
      "setresgid",
      "setresuid",
      "setgid",
      "setuid",
      "shutdown",
      "socket",
      "vfork",
	}
)

type SyscallTestSuite struct {
	IntegrationTestSuiteBase
	syscall string
}

func TestCollectorProbeSyscalls(t *testing.T) {
	for _, syscall := range syscalls {
		test := new(SyscallTestSuite)
		test.syscall = syscall

		suite.Run(t, test)
	}
}

func (s *SyscallTestSuite) SetupSuite() {

}

func (s *SyscallTestSuite) TearDownSuite() {

}

func (s *SyscallTestSuite) TestSyscallEvent() {

}