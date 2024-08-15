package types

import "sort"

type EndpointInfo struct {
	Protocol       string
	Address        ListenAddress
	CloseTimestamp string
	Originator     ProcessOriginator
}

func (n *EndpointInfo) IsActive() bool {
	// no close timestamp means the connection is open, and active
	return n.CloseTimestamp == NilTimestamp
}

func (n *EndpointInfo) Less(other EndpointInfo) bool {
	addr1, addr2 := n.Address, other.Address

	if !addr1.Equal(addr2) {
		return addr1.Less(addr2)
	}

	process1, process2 := n.Originator, other.Originator

	if !process1.Equal(process2) {
		return process1.Less(process2)
	}

	return false
}

func (n *EndpointInfo) Equal(other EndpointInfo) bool {
	return n.Address.Equal(other.Address) &&
		n.Originator.Equal(other.Originator) &&
		n.IsActive() == other.IsActive()
}

func SortEndpoints(endpoints []EndpointInfo) {
	sort.Slice(endpoints, func(i, j int) bool { return endpoints[i].Less(endpoints[j]) })
}
