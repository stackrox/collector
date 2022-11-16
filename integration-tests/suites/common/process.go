package common

import (
	"fmt"
	"strconv"
	"strings"
)

type ProcessInfo struct {
	Name    string
	ExePath string
	Uid     int
	Gid     int
	Pid     int
	Args    string
}

func NewProcessInfo(line string) (*ProcessInfo, error) {
	parts := strings.SplitN(line, ":", 6)

	if len(parts) != 6 {
		return nil, fmt.Errorf("invalid gRPC string for process info: %s", line)
	}

	uid, err := strconv.Atoi(parts[2])
	if err != nil {
		return nil, err
	}

	gid, err := strconv.Atoi(parts[3])
	if err != nil {
		return nil, err
	}

	pid, err := strconv.Atoi(parts[4])
	if err != nil {
		return nil, err
	}

	return &ProcessInfo{
		Name:    parts[0],
		ExePath: parts[1],
		Uid:     uid,
		Gid:     gid,
		Pid:     pid,
		Args:    parts[5],
	}, nil
}

type ProcessLineage struct {
	Name          string
	ExePath       string
	ParentUid     int
	ParentExePath string
}

func NewProcessLineage(line string) (*ProcessLineage, error) {
	parts := strings.SplitN(line, ":", 6)

	if len(parts) != 6 || parts[2] != "ParentUid" || parts[4] != "ParentExecFilePath" {
		return nil, fmt.Errorf("invalid gRPC string for process lineage: %s", line)
	}

	parentUid, err := strconv.Atoi(parts[3])
	if err != nil {
		return nil, err
	}

	return &ProcessLineage{
		Name:          parts[0],
		ExePath:       parts[1],
		ParentUid:     parentUid,
		ParentExePath: parts[5],
	}, nil
}
