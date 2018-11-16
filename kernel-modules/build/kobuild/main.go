package main

import (
	"flag"
	"fmt"
	"os"
	"strconv"

	"github.com/stackrox/collector/kernel-modules/build/kobuild/command"
	"github.com/stackrox/collector/kernel-modules/build/kobuild/config"
)

var (
	circleNodeTotal = getEnvVar("CIRCLE_NODE_TOTAL", 1)
	circleNodeIndex = getEnvVar("CIRCLE_NODE_INDEX", 0)
)

func main() {
	if err := mainCmd(); err != nil {
		fmt.Fprintf(os.Stderr, "kobuild: %s\n", err.Error())
		os.Exit(1)
	}
}

func mainCmd() error {
	configFlag := flag.String("config", "kernel-manifest.yml", "Config file containing build manifest")
	flag.Parse()

	builders, err := config.Load(*configFlag)
	if err != nil {
		return err
	}

	manifests, err := builders.Manifests()
	if err != nil {
		return err
	}

	markManifests(manifests)
	return buildManifests(manifests)
}

// markManifests examines each manifest and marks if a given manifest should be
// built on the current CircleCI node.
func markManifests(manifests []*config.Manifest) {
	for index, manifest := range manifests {
		if index%circleNodeTotal == circleNodeIndex {
			manifest.Build = true
		} else {
			manifest.Build = false
		}
	}
}

func buildManifests(manifests []*config.Manifest) error {

	for index, manifest := range manifests {
		if manifest.Build == false {
			fmt.Printf("Skipping build of manifest %d/%d\n\n", index+1, len(manifests))
			continue
		}

		fmt.Printf("Starting build of manifest %d/%d\n", index+1, len(manifests))
		fmt.Printf("%s.%s (%s)\n", manifest.Kind, manifest.Builder, manifest.Description)
		fmt.Printf("Files:\n")
		for _, pkg := range manifest.Packages {
			fmt.Printf("  - %s\n", pkg)
		}
		args := []string{
			manifest.Kind,
		}
		args = append(args, manifest.Packages...)
		fmt.Printf("Command:\n")
		fmt.Println("  ./build-kos")
		for _, arg := range args {
			fmt.Printf("    %s\n", arg)
		}

		if err := command.Run("build-kos", args...); err != nil {
			fmt.Printf("Failed build of manifest %d/%d\n\n", index+1, len(manifests))
			return err
		}

		fmt.Printf("Finished build of manifest %d/%d\n\n", index+1, len(manifests))
	}

	return nil
}

// getEnvVar looks up the variable named by key in the current environment and
// converts it into an integer. If the variable is not found or cannot be
// converted, the fallback value is returned instead.
func getEnvVar(key string, fallback int) int {
	value, found := os.LookupEnv(key)
	if !found {
		return fallback
	}

	parsed, err := strconv.Atoi(value)
	if err != nil {
		return fallback
	}

	return parsed
}
