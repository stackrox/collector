package command

import (
	"fmt"
	"io"
	"os"
	"os/exec"

	"github.com/kballard/go-shellquote"
)

// Run will exec the given command and stream all output (stdout and stderr)
// back to the current terminal.
func Run(printOnly bool, name string, arg ...string) error {
	if printOnly {
		allArgs := make([]string, len(arg) + 1)
		allArgs[0] = name
		copy(allArgs[1:], arg)
		fmt.Println(shellquote.Join(allArgs...))
		return nil
	}

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
