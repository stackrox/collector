package common

import (
	"fmt"
	"regexp"
	"strings"
)

type ProcessOriginator struct {
	ProcessName         string
	ProcessExecFilePath string
	ProcessArgs         string
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

	if line == "\n" {
		return nil, fmt.Errorf("ProcessOriginator is empty")
	}

	var processArgs string
	r := regexp.MustCompile("process_name:(.*)process_exec_file_path:(.*)process_args:(.*)\n$")
	processArr := r.FindStringSubmatch(line)
	if len(processArr) != 4 {
		r := regexp.MustCompile("process_name:(.*)process_exec_file_path:(.*)\n$")
		processArr = r.FindStringSubmatch(line)
		if len(processArr) != 3 {
			return nil, fmt.Errorf("Could not parse process originator %s", line)
		}
	} else {
		processArgs = removeQuotesAndWhiteSpace(processArr[3])
	}
	processName := removeQuotesAndWhiteSpace(processArr[1])
	processExecFilePath := removeQuotesAndWhiteSpace(processArr[2])

	return &ProcessOriginator{
		ProcessName:         processName,
		ProcessExecFilePath: processExecFilePath,
		ProcessArgs:         processArgs,
	}, nil
}
