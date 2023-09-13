package types

import (
	"fmt"
	"strings"
)

type NetworkInfo struct {
	LocalAddress   string
	RemoteAddress  string
	Role           string
	SocketFamily   string
	CloseTimestamp string
}

func NewNetworkInfo(line string) (*NetworkInfo, error) {
	parts := strings.Split(line, "|")

	if len(parts) != 5 {
		return nil, fmt.Errorf("Invalid gRPC string for network info: %s", line)
	}

	return &NetworkInfo{
		LocalAddress:   parts[0],
		RemoteAddress:  parts[1],
		Role:           parts[2],
		SocketFamily:   parts[3],
		CloseTimestamp: parts[4],
	}, nil
}

func (n *NetworkInfo) IsActive() bool {
	// no close timestamp means the connection is open, and active
	return n.CloseTimestamp == "(timestamp: nil Timestamp)"
}
