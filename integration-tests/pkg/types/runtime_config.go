package types

import (
	"gopkg.in/yaml.v3"
)

type RuntimeConfig struct {
	Networking struct {
		ExternalIps struct {
			Enabled string `yaml:"enabled"`
		} `yaml:"externalIps"`
	} `yaml:"networking"`
}

func (n *RuntimeConfig) Equal(other RuntimeConfig) bool {
	return n.Networking.ExternalIps.Enabled == other.Networking.ExternalIps.Enabled
}

func (n *RuntimeConfig) GetRuntimeConfigStr() (string, error) {
	yamlBytes, err := yaml.Marshal(n)

	if err != nil {
		return "", err
	}

	return string(yamlBytes), err
}
