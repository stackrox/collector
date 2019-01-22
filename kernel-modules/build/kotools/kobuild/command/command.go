package command

import (
	"io"
	"os"
	"os/exec"
)

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

	go io.Copy(os.Stdout, stdout)
	go io.Copy(os.Stderr, stderr)

	return cmd.Run()
}
