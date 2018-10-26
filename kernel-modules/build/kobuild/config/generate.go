package config

import (
	"bufio"
	"fmt"
	"os"
	"regexp"
	"strings"
)

var rhre = regexp.MustCompile(`/kernel-(?:lt-)?devel-([.0-9]+-[.0-9]+)\.([.a-z0-9]+\.x86_64)\.rpm`)

var sources = map[string]struct{
	file        string
	description string
	kind        string
	versionTag  string
	pattern     *regexp.Regexp
}{
	"amazon": {
		"amazon.txt", "AWS kernels", "RedHat", "amzn2.x86_64",regexp.MustCompile(`/kernel-devel-([-.0-9]+).amzn2.x86_64.rpm`),
	},
	"centos": {
		"centos.txt", "CentOS kernels", "RedHat", "", rhre,
	},
	"centos-uncrawled": {
		"centos-uncrawled.txt", "CentOS kernels", "RedHat", "", rhre,
	},
	"coreos": {
		"coreos.txt", "CoreOS kernels", "CoreOS", "coreos", regexp.MustCompile(`/amd64-usr/([^/]+)/`),
	},
	"kops": {
		"kops.txt", "KOps kernels", "Debian", "k8s", regexp.MustCompile(`/linux-headers-([0-9.]+)-k8s`),
	},
	"ubuntu-aws": {
		"ubuntu-aws.txt", "Ubuntu AWS kernels", "Ubuntu", "aws", regexp.MustCompile(`/linux(?:-aws)?-headers-([0-9.]+-[0-9]+)(-aws)?_`),
	},
	"ubuntu-azure": {
		"ubuntu-azure.txt", "Ubuntu Azure kernels", "Ubuntu", "aws", regexp.MustCompile(`/linux(?:-azure)?-headers-([0-9.]+-[0-9]+)(-azure)?_`),
	},
	"ubuntu-gcp": {
		"ubuntu-gcp.txt", "Ubuntu GCP kernels", "Ubuntu", "aws", regexp.MustCompile(`/linux(?:-gcp)?-headers-([0-9.]+-[0-9]+)(-gcp)?_`),
	},
	"ubuntu-gke": {
		"ubuntu-gke.txt", "Ubuntu GKE kernels", "Ubuntu", "aws", regexp.MustCompile(`/linux(?:-gke)?-headers-([0-9.]+-[0-9]+)(-gke)?_`),
	},
	"ubuntu-standard": {
		"ubuntu-standard.txt", "Ubuntu kernels", "Ubuntu", "generic", regexp.MustCompile(`/linux-headers-([0-9.]+-[0-9]+)(-generic)?_`),
	},
}

func readLines(filename string) ([]string, error) {
	f, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	var lines []string
	scan := bufio.NewScanner(f)
	for scan.Scan() {
		lines = append(lines, scan.Text())
	}
	return lines, nil
}

func appendValue(builder *Builder, key1, key2, value string) {
	if builder.Versions == nil {
		builder.Versions = make(map[string]map[string][]string)
	}
	m := builder.Versions[key1]
	if m == nil {
		m = make(map[string][]string)
		builder.Versions[key1] = m
	}
	m[key2] = append(m[key2], value)
}

func Generate() (Builders, error) {
	builders := Builders{}
	for s, val := range sources {
		b := Builder{
			Description: val.description,
			Kind: val.kind,
		}
		files, err := readLines(val.file)
		if err != nil {
			return builders, err
		}
		first := ""
		if val.kind == "Debian" {
			first = files[0]
			files = files[1:]
		}
		for _, f := range files {
			f = strings.Replace(f, "//", "/", -1)
			match := val.pattern.FindStringSubmatch(f)
			if match == nil {
				err = fmt.Errorf("URL %q doesn't match regexp %q", f, val.pattern.String())
				panic(err)
				return builders, err
			}
			versionTag := val.versionTag
			if val.versionTag == "" {
				versionTag = match[2]
			}
			if first != "" {
				appendValue(&b, versionTag, match[1], first)
			}
			appendValue(&b, versionTag, match[1], f)
		}
		builders[s] = b
	}
	return builders, nil
}
