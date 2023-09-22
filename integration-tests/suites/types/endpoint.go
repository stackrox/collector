package types

type EndpointInfo struct {
	Protocol       string
	Address        *ListenAddress
	CloseTimestamp string
	Originator     *ProcessOriginator
}
