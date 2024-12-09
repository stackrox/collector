package types

import (
	"gopkg.in/yaml.v3"
)

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

func (n *RuntimeConfig) GetRuntimeConfigStr() (string, error) {
	yamlBytes, err := yaml.Marshal(n)

	if err != nil {
		return "", err
	}

	return string(yamlBytes), err
}
