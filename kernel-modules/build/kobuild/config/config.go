package config

import (
	"bytes"
	"crypto/sha256"
	"fmt"
	"io/ioutil"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"

	"gopkg.in/yaml.v2"
)

// Builders represents a named map of Builder instances.
type Builders map[string]Builder

type ReformatterFunc func(builder Builder, packages []string) ([][]string, error)

// Builder represents a single builder configuration.
type Builder struct {
	Description string   `yaml:"description"`
	File        string   `yaml:"file"`
	Kind        string   `yaml:"type"`
	Name        string   `yaml:"-"`
	Packages    []string `yaml:"-"`
	Reformat    string   `yaml:"reformat"`
}

// Manifest represents a fully self-contained kernel build unit. All
// information required for building a single kernel module is captured in a
// Manifest instance.
type Manifest struct {
	Builder     string
	Description string
	Packages    []string
	Kind        string
	Build       bool
}

// checksumPackageNames returns a consistent hash for the given set of package names.
func checksumPackageNames(packages []string) string {
	var (
		s   = sha256.New()
	)

	sort.Strings(packages)
	for _, pkg := range packages {
		s.Write([]byte(pkg))
	}

	return fmt.Sprintf("%x", s.Sum(nil))
}

// Fullname returns a unique name for the current manifest. This name is
// intended to be used for lexicographically sorting multiple manifests.
func (m *Manifest) Fullname() string {
	sum := checksumPackageNames(m.Packages)
	return fmt.Sprintf("%s-%s-%s", m.Kind, m.Builder, sum)
}

// Load reads the given filename as yaml and parses the content into a list of
// Builders.
func Load(filename string) (Builders, error) {
	body, err := ioutil.ReadFile(filename)
	if err != nil {
		return nil, err
	}

	var builders map[string]Builder
	if err := yaml.UnmarshalStrict(body, &builders); err != nil {
		return nil, err
	}

	configDir := filepath.Dir(filename)

	for name, builder := range builders {
		packageFilePath := filepath.Join(configDir, builder.File)
		packages, err := readPackagesFile(packageFilePath)
		if err != nil {
			return nil, err
		}

		builder.Packages = packages
		builder.Name = name
		builders[name] = builder
	}

	return builders, nil
}

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

// Manifests iterates across the given Builders and returns a consistently
// ordered list of build manifests as a cross-product.
func (b *Builders) Manifests() ([]*Manifest, error) {
	var manifests = make([]*Manifest, 0, 512)

	for _, builder := range *b {
		reformatters := map[string]ReformatterFunc{
			"coreos":       ReformatCoreOS,
			"one-to-each":  ReformatOneToEach,
			"one-to-pairs": ReformatOneToPairs,
			"pairs":        ReformatPairs,
			"single":       ReformatSingle,
		}

		reformatterFunc, found := reformatters[builder.Reformat]
		if !found {
			return nil, fmt.Errorf("unknown reformatter %q", builder.Reformat)
		}

		// Group packages together using their specific reformatter.
		sets, err := reformatterFunc(builder, builder.Packages)
		if err != nil {
			return nil, err
		}

		// Create a build manifest for each group of packages.
		for _, set := range sets {
			manifest := &Manifest{
				Packages:    set,
				Description: builder.Description,
				Kind:        builder.Kind,
				Builder:     builder.Name,
				Build:       false,
			}

			manifests = append(manifests, manifest)
		}
	}

	// Deterministically sort build manifests by their full name.
	sort.SliceStable(manifests, func(i, j int) bool {
		return manifests[i].Fullname() < manifests[j].Fullname()
	})

	return manifests, nil
}

// ReformatCoreOS consumes a list of packages, and returns a list of package
// groups. Each package group is comprised of a single input package. Each
// package is given the suffix "/bundle.tgz".
//
// For example:
// [a, b, c] → [[a/bundle.tgz], [b/bundle.tgz], [c/bundle.tgz]]
func ReformatCoreOS(builder Builder, packages []string) ([][]string, error) {
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
func ReformatOneToEach(builder Builder, packages []string) ([][]string, error) {
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
func ReformatOneToPairs(builder Builder, packages []string) ([][]string, error) {
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
func ReformatPairs(builder Builder, packages []string) ([][]string, error) {
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
func ReformatSingle(builder Builder, packages []string) ([][]string, error) {
	var sets = make([][]string, 0, len(packages))

	for _, pkg := range packages {
		set := []string{pkg}
		sets = append(sets, set)
	}

	return sets, nil
}
