package main

import (
	"flag"
	"fmt"
	"os"
	"strconv"

	"errors"
	"sort"

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
	var (
		configFlag   = flag.String("config", "kernel-manifest.yml", "Config file containing build manifest")
		buildFlag    = flag.Bool("build", false, "Build modules")
		packagesFlag = flag.Bool("packages", false, "List packages")
	)
	flag.Parse()

	builders, err := config.Load(*configFlag)
	if err != nil {
		return err
	}

	manifests := builders.Manifests()
	markManifests(manifests)

	switch {
	case *buildFlag:
		return buildManifests(manifests)
	case *packagesFlag:
		listPackages(manifests)
		return nil
	default:
		return errors.New("no action given")
	}
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
		fmt.Printf("%s version %s-%s (%s)\n", manifest.Builder, manifest.Version, manifest.Flavor, manifest.Kind)
		fmt.Printf("  %s\n", manifest.Description)
		fmt.Printf("Files:\n")
		for _, pkg := range manifest.Packages {
			fmt.Printf("  - %s\n", pkg)
		}
		args := []string{
			manifest.Kind, manifest.Version, manifest.Flavor,
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

func listPackages(manifests []*config.Manifest) {
	var (
		packageSet  = make(map[string]struct{}, len(manifests))
		packageList = make([]string, 0, len(manifests))
	)

	// Add each package for every (relevant) manifest to the package set
	for _, manifest := range manifests {
		if manifest.Build == false {
			continue
		}
		for _, pkg := range manifest.Packages {
			packageSet[pkg] = struct{}{}
		}
	}

	// Transform the package set into a list of (unordered) packages
	for pkg := range packageSet {
		packageList = append(packageList, pkg)
	}

	// Sort the list of unordered packages
	sort.Strings(packageList)

	for _, pkg := range packageList {
		fmt.Println(pkg)
	}
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
