package integrationtests

import (
	//"strings"
	"fmt"
	"regexp"
)

type ProcessOriginator struct {
	ProcessName string
	ProcessExecFilePath string
	ProcessArgs string
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
	processName := processNameArr[1]
	fmt.Println("processName= " + processName)

	r = regexp.MustCompile("process_exec_file_path:(.*)process_args:")
	processExecFilePathArr := r.FindStringSubmatch(line)
	if len(processExecFilePathArr) !=2 {
		return nil, fmt.Errorf("Could not find process_exec_file_path in %s", line)
	}
	processExecFilePath := processExecFilePathArr[1]
	fmt.Println("processExecFilePath= " + processExecFilePath)

	r = regexp.MustCompile("process_args:(.*)$")
	processArgsArr := r.FindStringSubmatch(line)
	if len(processArgsArr) !=2 {
		return nil, fmt.Errorf("Could not find process_args in %s", line)
	}
	processArgs := processArgsArr[1]
	fmt.Println("processArgs= " + processArgs)


	return &ProcessOriginator {
		ProcessName: processName,
		ProcessExecFilePath: processExecFilePath,
		ProcessArgs: processArgs,
	}, nil
}
