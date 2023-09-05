package types

import (
	"fmt"
	"strings"
)

type EndpointInfo struct {
	Protocol       string
	Address        *ListenAddress
	CloseTimestamp string
	Originator     *ProcessOriginator
}

func NewEndpointInfo(line string) (*EndpointInfo, error) {
	parts := strings.Split(line, "|")

	if len(parts) != 5 {
		return nil, fmt.Errorf("Invalid gRPC string for endpoint info: %s", line)
	}

	originator, err := NewProcessOriginator(parts[4])

	if err != nil {
		return nil, err
	}

	listenAddress, err := NewListenAddress(parts[2])

	if err != nil {
		return nil, err
	}

	return &EndpointInfo{
		Protocol:       parts[1],
		Address:        listenAddress,
		CloseTimestamp: parts[3],
		Originator:     originator,
	}, nil
}
