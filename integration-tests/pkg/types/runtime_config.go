package types

import (
	"gopkg.in/yaml.v3"
)

type ExternalIpsConfig struct {
	Enabled   string `yaml:"enabled"`
	Direction string `yaml:"direction,omitempty"`
}

type NetworkConfig struct {
	ExternalIps ExternalIpsConfig `yaml:"externalIps"`
}

type RuntimeConfig struct {
	Networking NetworkConfig `yaml:"networking"`
}

// e.g.
//    runtimeConfig := types.RuntimeConfig {
//        Networking: types.NetworkConfig {
//            ExternalIps: types.ExternalIpsConfig {
//                Enabled: "ENABLED"
//            },
//        },
//    }

func (n *RuntimeConfig) Equal(other RuntimeConfig) bool {
	return n.Networking.ExternalIps.Enabled == other.Networking.ExternalIps.Enabled
}

func (n *RuntimeConfig) String() string {
	yamlBytes, err := yaml.Marshal(n)

	if err != nil {
		panic(err)
	}

	return string(yamlBytes)
}
