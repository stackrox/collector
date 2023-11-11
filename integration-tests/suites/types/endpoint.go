package types

import "sort"

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

func SortEndpoints(endpoints []EndpointInfo) {
	sort.Slice(endpoints, func(i, j int) bool { return endpointComparison(endpoints[i], endpoints[j]) })
}

func endpointComparison(endpoint1 EndpointInfo, endpoint2 EndpointInfo) bool {
	addr1, addr2 := endpoint1.Address, endpoint2.Address

	if addr1 == nil {
		return false
	}
	if addr2 == nil {
		return true
	}

	if addr1.AddressData != addr2.AddressData {
		return addr1.AddressData < addr2.AddressData
	}

	if addr1.Port != addr2.Port {
		return addr1.Port < addr2.Port
	}

	if endpoint1.Protocol != endpoint2.Protocol {
		return endpoint1.Protocol < endpoint2.Protocol
	}

	process1, process2 := endpoint1.Originator, endpoint2.Originator

	if process1 == nil {
		return false
	}

	if process2 == nil {
		return true
	}

	if process1.ProcessName != process2.ProcessName {
		return process1.ProcessName < process2.ProcessName
	}

	if process1.ProcessExecFilePath != process2.ProcessExecFilePath {
		return process1.ProcessExecFilePath < process2.ProcessExecFilePath
	}

	if process1.ProcessArgs != process2.ProcessArgs {
		return process1.ProcessArgs < process2.ProcessArgs
	}

	return false
}
