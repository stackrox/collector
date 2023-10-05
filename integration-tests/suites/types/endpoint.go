package types

type EndpointInfo struct {
	Protocol       string
	Address        *ListenAddress
	CloseTimestamp string
	Originator     *ProcessOriginator
}

func (n *EndpointInfo) IsActive() bool {
	// no close timestamp means the connection is open, and active
	return n.CloseTimestamp == NilTimestamp
}
