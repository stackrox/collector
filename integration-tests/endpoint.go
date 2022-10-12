package integrationtests

import (
	"strings"
	"fmt"
)

type EndpointInfo struct {
	Protocol string
	ListenAddress string
	CloseTimestamp string
	Originator string
}

func NewEndpointInfo(line string) (*EndpointInfo, error) {
	parts := strings.Split(line, "|")

	if len(parts) != 5 {
		return nil, fmt.Errorf("Invalid gRPC string for endpoint info: %s", line)
	}

	return &EndpointInfo {
		Protocol: parts[1],
		ListenAddress: parts[2],
		CloseTimestamp: parts[3],
		Originator: parts[4],
	}, nil
}
