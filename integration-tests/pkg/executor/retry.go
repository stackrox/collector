package executor

import (
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

const (
	max_retries     = 5
	retry_wait_time = 3 * time.Second

	tickSeconds = time.Second
	timeout     = 30 * time.Second

	NoOutput = ""
)

type retryable = func() (string, error)
type errorchecker = func(string, error) error

// Simple retry in loop for commands produce only one string output and error
func Retry(f retryable) (output string, err error) {
	return RetryWithErrorCheck(func(s string, e error) error { return e }, f)
}

// Simple retry with error checker
func RetryWithErrorCheck(ec errorchecker, f retryable) (output string, err error) {
	for i := 0; i < max_retries; i++ {
		output, err = f()
		if ec(output, err) == nil {
			return output, nil
		} else if i != max_retries-1 {
			log.Error("Retrying (%d of %d) Error: %v\n", i, max_retries, err)
			common.Sleep(retry_wait_time)
		}
	}

	return output, err
}

// Retry based on a ticker with timeout.
//
// Note that the caller is responsible for reporting outstanding errors in
// ticker function
func RetryWithTimeout(f retryable, timeoutMsg error) (
	output string, err error) {
	tick := time.Tick(tickSeconds)
	timer := time.After(timeout)

	for {
		select {
		case <-tick:
			output, err := f()
			if err == nil {
				return output, nil
			}
		case <-timer:
			return NoOutput, timeoutMsg
		}
	}
}
