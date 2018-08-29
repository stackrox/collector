package command

import (
	"io"
	"os"
	"os/exec"
)

// stream shuttles data from input to output until exhausted.
func stream(input io.ReadCloser, output *os.File) {
	buffer := make([]byte, 1024)
	defer input.Close()
	for {
		n, err := input.Read(buffer)
		if err != nil {
			return
		}
		output.Write(buffer[0:n])
	}
}

// Run will exec the given command and stream all output (stdout and stderr)
// back to the current terminal.
func Run(name string, arg ...string) error {
	cmd := exec.Command(name, arg...)

	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}

	stderr, err := cmd.StderrPipe()
	if err != nil {
		return err
	}

	go stream(stdout, os.Stdout)
	go stream(stderr, os.Stderr)

	return cmd.Run()
}
