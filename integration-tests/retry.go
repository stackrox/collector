package integrationtests

import (
	"time"
)

const (
	max_retries     = 5
	retry_wait_time = 3 * time.Second
)

type retryable = func() (string, error)

// Simple retry in loop for commands produce only one string output and error
func retry(f retryable) (output string, err error) {
	for i := 0; i < max_retries; i++ {
		output, err = f()
		if err == nil {
			return output, nil
		} else {
			time.Sleep(retry_wait_time)
		}
	}

	return output, err
}
