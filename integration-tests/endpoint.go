package integrationtests

import (
	"strings"
	"fmt"
)

type EndpointInfo struct {
	Protocol string
	ListenAddress string
	CloseTimestamp string
}

func NewEndpointInfo(line string) (*EndpointInfo, error) {
	parts := strings.Split(line, "|")

	if len(parts) != 4 {
		return nil, fmt.Errorf("invalid gRPC string for endpoint info: %s", line)
	}

	return &EndpointInfo {
		Protocol: parts[1],
		ListenAddress: parts[2],
		CloseTimestamp: parts[3],
	}, nil
}
