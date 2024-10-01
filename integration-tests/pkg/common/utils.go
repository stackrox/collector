package common

import (
	"bytes"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"golang.org/x/sys/unix"
	"k8s.io/utils/strings/slices"
)

// containerShortID returns the first twelve character of a containerID
// to match the shortened IDs returned by docker.
func ContainerShortID(containerID string) string {
	return containerID[0:12]
}

// quoteArgs will add quotes around any arguments require it for the shell.
func QuoteArgs(args []string) []string {
	quotedArgs := []string{}
	for _, arg := range args {
		argQuoted := strconv.Quote(arg)
		argQuotedTrimmed := strings.Trim(argQuoted, "\"")
		if arg != argQuotedTrimmed || strings.Contains(arg, " ") {
			arg = argQuoted
		}
		quotedArgs = append(quotedArgs, arg)
	}
	return quotedArgs
}

// Returns the min of two integers.
// Strangely there is no such built in or function in math
func Min(x int, y int) int {
	if x < y {
		return x
	}
	return y
}

// Identifies if the current architecture is in the specified supported list.
// Returns a boolean flag indicatind the result, and the actual architecture,
// that was discovered.
func ArchSupported(supported ...string) (bool, string) {
	u := unix.Utsname{}
	unix.Uname(&u)

	// Exclude null bytes from comparison
	arch := string(bytes.Trim(u.Machine[:], "\x00"))
	return slices.Contains(supported, arch), arch
}

// Creates a new file to dump logs into
func PrepareLog(testName string, logName string) (*os.File, error) {
	pathSections := []string{
		".", "container-logs",
		config.VMInfo().Config,
		config.CollectionMethod(),
		testName,
	}

	logDirectory := filepath.Join(pathSections...)
	err := os.MkdirAll(logDirectory, os.ModePerm)
	if err != nil {
		return nil, err
	}

	logPath := filepath.Join(logDirectory, logName)
	return os.Create(logPath)
}

func Sleep(duration time.Duration) {
	_, filename, line, _ := runtime.Caller(1)
	log.Info("[%s:%d] sleep %f s", filepath.Base(filename), line, duration.Seconds())
	time.Sleep(duration)
}
