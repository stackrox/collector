package types

const (
	NilTimestamp = "<nil>"
)

type NetworkInfo struct {
	LocalAddress   string
	RemoteAddress  string
	Role           string
	SocketFamily   string
	CloseTimestamp string
}

func (n *NetworkInfo) IsActive() bool {
	// no close timestamp means the connection is open, and active
	return n.CloseTimestamp == NilTimestamp
}

func (n *NetworkInfo) Equal(other NetworkInfo) bool {
	return n.LocalAddress == other.LocalAddress &&
		n.RemoteAddress == other.RemoteAddress &&
		n.Role == other.Role &&
		n.SocketFamily == other.SocketFamily &&
		n.IsActive() == other.IsActive()
}
