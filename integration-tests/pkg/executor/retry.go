package executor

import (
	"time"
)

const (
	max_retries     = 5
	retry_wait_time = 3 * time.Second
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
			time.Sleep(retry_wait_time)
		}
	}

	return output, err
}
