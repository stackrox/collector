package types

const (
	NilTimestamp = "(timestamp: nil Timestamp)"
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
