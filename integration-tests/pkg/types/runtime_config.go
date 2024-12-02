package types

type RuntimeConfig struct {
	Networking struct {
		ExternalIps struct {
			Enable bool `yaml:"enable"`
		} `yaml:"externalIps"`
	} `yaml:"networking"`
}

func (n *RuntimeConfig) Equal(other RuntimeConfig) bool {
	return n.Networking.ExternalIps.Enable == other.Networking.ExternalIps.Enable
}
