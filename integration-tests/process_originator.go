package integrationtests

import (
	"strings"
	"fmt"
	"regexp"
)

type ProcessOriginator struct {
	ProcessName string
	ProcessExecFilePath string
	ProcessArgs string
}

func removeQuotesAndWhiteSpace(s string) string {
	s = strings.TrimSpace(s)
	if len(s) > 0 && s[0] == '"' {
		s = s[1:]
	}
	if len(s) > 0 && s[len(s)-1] == '"' {
		s = s[:len(s)-1]
	}
	return s
}

func NewProcessOriginator(line string) (*ProcessOriginator, error) {
	if line == "<nil>\n" {
		return nil, fmt.Errorf("ProcessOriginator is nil")
	}

	r := regexp.MustCompile("process_name:(.*)process_exec_file_path:")
	processNameArr := r.FindStringSubmatch(line)
	if len(processNameArr) !=2 {
		return nil, fmt.Errorf("Could not find process_name in %s", line)
	}
	processName := removeQuotesAndWhiteSpace(processNameArr[1])

	r = regexp.MustCompile("process_exec_file_path:(.*)process_args:")
	processExecFilePathArr := r.FindStringSubmatch(line)
	if len(processExecFilePathArr) !=2 {
		r = regexp.MustCompile("process_exec_file_path:(.*)\n$")
		processExecFilePathArr = r.FindStringSubmatch(line)
		if len(processExecFilePathArr) !=2 {
			return nil, fmt.Errorf("Could not find process_exec_file_path in %s", line)
		}
	}
	processExecFilePath := removeQuotesAndWhiteSpace(processExecFilePathArr[1])

	r = regexp.MustCompile("process_args:(.*)\n$")
	processArgsArr := r.FindStringSubmatch(line)
	var processArgs string
	if len(processArgsArr) !=2 {
		processArgs = ""
	} else {
		processArgs = removeQuotesAndWhiteSpace(processArgsArr[1])
	}

	return &ProcessOriginator {
		ProcessName: processName,
		ProcessExecFilePath: processExecFilePath,
		ProcessArgs: processArgs,
	}, nil
}
