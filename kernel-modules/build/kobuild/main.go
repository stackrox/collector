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

func log(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, format, args...)
	fmt.Fprintln(os.Stderr)
}

func main() {
	if err := mainCmd(); err != nil {
		log("kobuild: %v", err)
		os.Exit(1)
	}
}

func mainCmd() error {
	configFlag := flag.String("config", "kernel-manifest.yml", "Config file containing build manifest")
	printPackagesFlag := flag.Bool("print-pkgs-only", false, "Only print required packages and exit")
	printCmdOnlyFlag := flag.Bool("print-commands-only", false, "Only print commands that need to be executed")
	existingVersionsFlag := flag.String("existing-versions", "", "File containing Kernel versions for which a module is already available")
	flag.Parse()

	builders, err := config.Load(*configFlag)
	if err != nil {
		return err
	}

	manifests := builders.Manifests()
	var existingVersions map[string]struct{}
	if existingVersionsFlag != nil && *existingVersionsFlag != "" {
		existingVersions, err = config.LoadExistingVersions(*existingVersionsFlag)
		if err != nil {
			return err
		}
	}
	markManifestsForMissingVersions(manifests, existingVersions)
	markManifestsForShardedBuild(manifests)

	if printPackagesFlag != nil && *printPackagesFlag {
		return printPackages(manifests)
	}
	printCmdOnly := false
	if printCmdOnlyFlag != nil {
		printCmdOnly = *printCmdOnlyFlag
	}
	return buildManifests(manifests, printCmdOnly)
}

func markManifestsForMissingVersions(manifests []*config.Manifest, existingVersions map[string]struct{}) {
	for _, manifest := range manifests {
		_, exists := existingVersions[manifest.KernelVersion()]
		manifest.Build = !exists
	}
}

// markManifests examines each manifest and marks if a given manifest should be
// built on the current CircleCI node.
func markManifestsForShardedBuild(manifests []*config.Manifest) {
	i := 0
	for _, manifest := range manifests {
		if !manifest.Build {
			continue
		}
		if i%circleNodeTotal == circleNodeIndex {
			manifest.Build = true
		} else {
			manifest.Build = false
		}
		i++
	}
}

func printPackages(manifests []*config.Manifest) error {
	for _, manifest := range manifests {
		if !manifest.Build {
			continue
		}
		for _, pkg := range manifest.Packages {
			fmt.Println(pkg)
		}
	}
	return nil
}


func buildManifests(manifests []*config.Manifest, printCmdOnly bool) error {
	for index, manifest := range manifests {
		if manifest.Build == false {
			continue
		}


		log("Starting build of manifest %d/%d", index+1, len(manifests))
		log("%s version %s-%s (%s)", manifest.Builder, manifest.Version, manifest.Flavor, manifest.Kind)
		log("  %s", manifest.Description)
		log("Files:")
		for _, pkg := range manifest.Packages {
			log("  - %s", pkg)
		}
		args := []string{
			manifest.Kind, manifest.Version, manifest.Flavor,
		}
		args = append(args, manifest.Packages...)
		log("Command:")
		log("  ./build-kos")
		for _, arg := range args {
			log("    %s", arg)
		}

		if err := command.Run(printCmdOnly, "build-kos", args...); err != nil {
			log("Failed build of manifest %d/%d\n", index+1, len(manifests))
			return err
		}

		log("Finished build of manifest %d/%d\n", index+1, len(manifests))
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
