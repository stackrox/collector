package types

type ExternalIps struct {
	Enable bool
}

type Networking struct {
	ExternalIps ExternalIps
}

type RuntimeConfig struct {
	Networking Networking
}

func (n *RuntimeConfig) Equal(other RuntimeConfig) bool {
	return n.Networking.ExternalIps.Enable == other.Networking.ExternalIps.Enable
}

//func (n *EndpointInfo) Equal(other EndpointInfo) bool {
//	return n.Address.Equal(other.Address) &&
//		n.Originator.Equal(other.Originator) &&
//		n.IsActive() == other.IsActive()
//}
