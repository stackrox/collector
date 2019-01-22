package main

import (
	"bytes"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"regexp"
	"strconv"

	"github.com/stackrox/collector/kernel-modules/build/kotools/kobuild/config"
	"gopkg.in/yaml.v2"
)

type (
	ReformatterFunc func(packages []string) ([][]string, error)

	Entry struct {
		Name        string `yml:"name"`
		Description string `yml:"description"`
		Type        string `yml:"type"`
		Reformat    string `yml:"reformat"`
		Version     string `yml:"version"`
		File        string `yml:"file"`
	}

	Config []Entry
)

var (
	reformatters = map[string]ReformatterFunc{
		"coreos":       ReformatCoreOS,
		"one-to-each":  ReformatOneToEach,
		"one-to-pairs": ReformatOneToPairs,
		"pairs":        ReformatPairs,
		"single":       ReformatSingle,
	}
)

func main() {
	if err := mainCmd(); err != nil {
		fmt.Fprintf(os.Stderr, "komanifest: %s\n", err.Error())
		os.Exit(1)
	}
}

func mainCmd() error {
	configFlag := flag.String("config", "reformat.yml", "Config file containing reformat manifest")
	flag.Parse()

	var (
		cfg       Config
		configDir = path.Dir(*configFlag)
	)

	body, err := ioutil.ReadFile(*configFlag)
	if err != nil {
		return err
	}

	if err := yaml.UnmarshalStrict(body, &cfg); err != nil {
		return err
	}

	builders := make(config.Builders, len(cfg))

	for _, entry := range cfg {
		pkgFile := path.Join(configDir, entry.File)

		packages, err := readPackagesFile(pkgFile)
		if err != nil {
			return err
		}

		reformatter := reformatters[entry.Reformat]
		packageSets, err := reformatter(packages)
		if err != nil {
			return err
		}

		builder := config.Builder{
			Description: entry.Description,
			Kind:        entry.Type,
			Packages:    make(map[string][]string, len(packageSets)),
		}

		for _, set := range packageSets {
			builder.Packages[config.ChecksumPackageNames(set)] = set
		}

		builders[entry.Name] = builder
	}

	rendered, err := yaml.Marshal(builders)
	if err != nil {
		return err
	}

	fmt.Printf("%s\n", rendered)
	return nil
}

// ReformatCoreOS consumes a list of packages, and returns a list of package
// groups. Each package group is comprised of a single input package. Each
// package is given the suffix "/bundle.tgz".
//
// For example:
// [a, b, c] → [[a/bundle.tgz], [b/bundle.tgz], [c/bundle.tgz]]
func ReformatCoreOS(packages []string) ([][]string, error) {
	var sets = make([][]string, 0, len(packages))

	for _, pkg := range packages {
		set := []string{pkg + "bundle.tgz"}
		sets = append(sets, set)
	}

	return sets, nil
}

// ReformatOneToEach consumes a list of packages, and returns a list of package
// groups. Each package group is comprised of the first package listed, and is
// paired with every package in turn.
//
// For example:
// [a, b, c] → [[a, b], [a, c]]
func ReformatOneToEach(packages []string) ([][]string, error) {
	var (
		sets  = make([][]string, 0, len(packages))
		first = packages[0]
	)

	for _, pkg := range packages[1:] {
		set := []string{first, pkg}
		sets = append(sets, set)
	}

	return sets, nil
}

// ReformatOneToPairs consumes a list of packages, and returns a list of
// package groups. Each package group is comprised of the first package listed,
// and a triple is made with every pair of packages in turn.
//
// For example:
// [a, b, c, d, e] → [[a, b, c], [a, d, e]]
func ReformatOneToPairs(packages []string) ([][]string, error) {
	if len(packages) < 3 || len(packages)%2 == 0 {
		panic("bad package count")
	}
	var (
		sets  = make([][]string, 0, len(packages))
		first = packages[0]
	)

	for index := 1; index < len(packages); index += 2 {
		set := []string{first, packages[index], packages[index+1]}
		sets = append(sets, set)
	}

	return sets, nil
}

// ReformatPairs consumes a list of packages, and returns a list of package
// groups. Each package group is comprised of pairs of packages with the same
// version string. Packages with newer revisions will replace older revisions.
//
// For example: (Notice that the ".40" revision was dropped in favor of the ".50".)
// [4.4.0-1031.40_amd64, 4.4.0-1031.40_all, 4.4.0-1031.50_amd64, 4.4.0-1031.50_all, 4.4.0-1069.79_amd64, 4.4.0-1069.79_all] →
// [[4.4.0-1031.50_amd64, 4.4.0-1031.50_all], [4.4.0-1069.79_amd64, 4.4.0-1069.79_all]]
func ReformatPairs(packages []string) ([][]string, error) {
	type rev struct {
		packages []string
		revision int
	}

	var (
		manifests = make([][]string, 0, len(packages)/2)
		reVersion = regexp.MustCompile(`(\d+\.\d+\.\d+-\d+)\.(\d+)`)
		versions  = make(map[string]rev)
	)

	for _, pkg := range packages {
		matches := reVersion.FindStringSubmatch(pkg)
		// Matches should have exactly 3 items, the full match, the version,
		// and the revision number.
		// Ex: {"4.4.0-1006.6", "4.4.0-1006", "6"}
		if len(matches) != 3 {
			return nil, fmt.Errorf("regex failed to match")
		}

		version := matches[1]
		revision, err := strconv.Atoi(matches[2])
		if err != nil {
			panic(err)
		}

		r, found := versions[version]

		switch {
		case found && r.revision > revision:
			break
		case found && r.revision == revision:
			r.packages = append(r.packages, pkg)
		case found && r.revision < revision:
			r = rev{[]string{pkg}, revision}
		case !found:
			r = rev{[]string{pkg}, revision}
		}

		versions[version] = r
	}

	for _, rev := range versions {
		// Sanity check, there should always be a pair of packages.
		if len(rev.packages) != 2 {
			return nil, fmt.Errorf("unpaired package %v", packages)
		}

		manifests = append(manifests, rev.packages)
	}

	return manifests, nil
}

// ReformatSingle consumes a list of packages, and returns a list of package
// groups. Each package group is comprised of a single input package.
//
// For example:
// [a, b, c] → [[a], [b], [c]]
func ReformatSingle(packages []string) ([][]string, error) {
	var sets = make([][]string, 0, len(packages))

	for _, pkg := range packages {
		set := []string{pkg}
		sets = append(sets, set)
	}

	return sets, nil
}

// readPackagesFile reads the given file and returns a list of all non-empty lines.
func readPackagesFile(filename string) ([]string, error) {
	body, err := ioutil.ReadFile(filename)
	if err != nil {
		return nil, err
	}

	lines := bytes.Split(body, []byte("\n"))
	packages := make([]string, 0, len(lines))
	for _, line := range lines {
		line = bytes.TrimSpace(line)
		if len(line) > 0 {
			packages = append(packages, string(line))
		}
	}

	return packages, nil
}
