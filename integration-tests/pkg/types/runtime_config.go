package types

type RuntimeConfig struct {
	Networking struct {
		ExternalIps struct {
			Enable bool
		}
	}
}

func (n *RuntimeConfig) Equal(other RuntimeConfig) bool {
	return n.Networking.ExternalIps.Enable == other.Networking.ExternalIps.Enable
}
