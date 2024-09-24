package executor

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"os"
	"strings"

	"github.com/docker/cli/cli/config/configfile"
	"github.com/docker/docker/api/types/registry"
	"github.com/docker/docker/client"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

type dockerRegistryConfig struct {
	enabled           bool
	encodedAuthConfig string
	authConfig        registry.AuthConfig
}

func readRegistryConfigs() (map[string]dockerRegistryConfig, error) {
	authConfigs := make(map[string]dockerRegistryConfig)
	for _, path := range config.RuntimeInfo().ConfigPaths {
		expanded := os.ExpandEnv(path)

		configBytes, err := os.ReadFile(expanded)
		if err != nil {
			log.Debug("unable to read config file: %s", err)
			continue
		}

		var dockerConfig configfile.ConfigFile
		if err := json.Unmarshal(configBytes, &dockerConfig); err != nil {
			log.Info("failed to parse config file: %s", err)
			continue
		}
		for server, dockerAuthConfig := range dockerConfig.AuthConfigs {
			authConfig := registry.AuthConfig{
				Username:      dockerAuthConfig.Username,
				Password:      dockerAuthConfig.Password,
				Auth:          dockerAuthConfig.Auth,
				ServerAddress: dockerAuthConfig.ServerAddress,
			}
			if len(authConfig.Auth) > 0 {
				authPlain, err := base64.StdEncoding.DecodeString(authConfig.Auth)
				if err != nil {
					return nil, err
				}
				split := strings.Split(string(authPlain), ":")
				authConfig.Username = split[0]
				authConfig.Password = split[1]
			}
			if authConfig.ServerAddress == "" {
				authConfig.ServerAddress = server
			}
			encodedAuthConfig, err := registry.EncodeAuthConfig(authConfig)
			if err != nil {
				return nil, err
			}
			authConfigs[server] = dockerRegistryConfig{enabled: false, authConfig: authConfig, encodedAuthConfig: encodedAuthConfig}
			log.Trace("read credentials for %s from %s", server, expanded)
		}
	}
	return authConfigs, nil
}

func getFullImageRef(ref string) (registry, repository, tag string) {
	registry = "docker.io"
	if strings.Contains(ref, ":") {
		parts := strings.Split(ref, ":")
		ref = parts[0]
		tag = parts[1]
	} else {
		tag = "latest"
	}
	if strings.Contains(ref, "/") {
		parts := strings.Split(ref, "/")
		if len(parts) == 3 || (len(parts) == 2 && strings.Contains(parts[0], ".")) {
			registry = parts[0]
			repository = strings.Join(parts[1:], "/")
		} else if len(parts) == 2 {
			repository = strings.Join(parts, "/")
		} else {
			repository = ref
		}
	} else {
		repository = "library/" + ref
	}
	return registry, repository, tag
}

func registryLoginAndEnable(cli *client.Client, registry *dockerRegistryConfig) error {
	if registry.enabled {
		log.Trace("registry %s enabled previously", registry.authConfig.ServerAddress)
		return nil
	}
	_, err := cli.RegistryLogin(context.Background(), registry.authConfig)
	if err != nil {
		return log.Error("Error logging into registry: %s %s", registry.authConfig.ServerAddress, err)
	}
	log.Info("registry login success: %s", registry.authConfig.ServerAddress)
	registry.enabled = true
	return nil
}
