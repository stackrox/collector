package config

type ContainerStartConfig struct {
	Name        string
	Image       string
	Privileged  bool
	NetworkMode string
	Mounts      map[string]string
	Env         map[string]string
	Command     []string
	Entrypoint  []string
	Ports       []uint16
}
