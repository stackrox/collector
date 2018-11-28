package config

import (
	"bufio"
	"os"
	"strings"
)

// LoadKernelVersions loads and parses a file containing Kernel versions, one per line.
func LoadKernelVersions(file string) (map[string]struct{}, error) {
	f, err := os.Open(file)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	versionsSet := make(map[string]struct{})
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		versionsSet[line] = struct{}{}
	}

	return versionsSet, nil
}
