package config

import (
	"crypto/sha256"
	"fmt"
	"io/ioutil"
	"net/url"
	"sort"
	"strings"

	"github.com/kballard/go-shellquote"
	"gopkg.in/yaml.v2"
)

// Builders represents a named map of Builder instances.
type Builders map[string]Builder

// Builder represents a single builder configuration. This captures all kernel
// versions that can be produced by the given type.
type Builder struct {
	Description string              `yaml:"description"`
	Kind        string              `yaml:"type"`
	Packages    map[string][]string `yaml:"packages"`
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

// ChecksumPackageNames returns a consistent hash for the given set of package names.
func ChecksumPackageNames(packages []string) string {
	var (
		s = sha256.New()
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
	return ChecksumPackageNames(m.Packages)
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

	return builders, nil
}

// Manifests iterates across the given Builders and returns a consistently
// ordered list of build manifests as a cross-product.
func (b *Builders) Manifests() []*Manifest {
	var manifests = make([]*Manifest, 0, 512)

	for name, builder := range *b {
		for _, packages := range builder.Packages {
			manifest := Manifest{
				Builder:     name,
				Description: builder.Description,
				Kind:        builder.Kind,
				Packages:    packages,
				Build:       false,
			}
			manifests = append(manifests, &manifest)
		}
	}

	sort.SliceStable(manifests, func(i, j int) bool {
		return manifests[i].Fullname() < manifests[j].Fullname()
	})

	return manifests
}

// BuildArgs returns the arguments passed to the ko builder as a string slice.
func (m *Manifest) BuildArgs() []string {
	args := []string{
		m.Kind,
	}
	args = append(args, m.URLEncodedPackages()...)
	return args
}

func (m *Manifest) URLEncodedPackages() []string {
	urlEncodedPackages := make([]string, len(m.Packages))
	for i, pkgUrl := range m.Packages {
		urlEncodedPackages[i] = url.PathEscape(pkgUrl)
	}
	return urlEncodedPackages
}

// BuildCommand returns the shell-escaped build command for the ko build, using the given
// builder (base) command.
func (m *Manifest) BuildCommand(cmdName string) string {
	args := m.BuildArgs()
	allArgs := make([]string, len(args)+1)
	allArgs[0] = cmdName
	copy(allArgs[1:], args)
	return shellquote.Join(allArgs...)
}

// PackageList returns the newline-separted list of packages required for the build as a
// single string.
func (m *Manifest) PackageList() string {
	return strings.Join(m.Packages, "\n")
}

func (m *Manifest) PackageListURLEncoded() string {
	return strings.Join(m.URLEncodedPackages(), "\n")
}
