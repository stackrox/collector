package types

type ListenAddress struct {
	AddressData string
	Port        int
	IpNetwork   string
}

func (l *ListenAddress) Equal(other ListenAddress) bool {
	return l.AddressData == other.AddressData &&
		l.Port == other.Port &&
		l.IpNetwork == other.IpNetwork
}

func (l *ListenAddress) Less(other ListenAddress) bool {
	return l.AddressData < other.AddressData ||
		l.Port < other.Port ||
		l.IpNetwork < other.IpNetwork
}
