package config

import (
	"bufio"
	"os"
	"strings"
)

// LoadExistingVersions loads and parses a file containing Kernel versions for which there
// already is a module, one per line.
func LoadExistingVersions(file string) (map[string]struct{}, error) {
	f, err := os.Open(file)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	existingVersions := make(map[string]struct{})
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		existingVersions[line] = struct{}{}
	}

	return existingVersions, nil
}