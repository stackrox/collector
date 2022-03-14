package integrationtests

import (
	"strconv"
	"strings"
)

// containerShortID returns the first twelve character of a containerID
// to match the shortened IDs returned by docker.
func containerShortID(containerID string) string {
	return containerID[0:12]
}

// quoteArgs will add quotes around any arguments require it for the shell.
func quoteArgs(args []string) []string {
	quotedArgs := []string{}
	for _, arg := range args {
		argQuoted := strconv.Quote(arg)
		argQuotedTrimmed := strings.Trim(argQuoted, "\"")
		if arg != argQuotedTrimmed || strings.Contains(arg, " ") {
			arg = argQuoted
		}
		quotedArgs = append(quotedArgs, arg)
	}
	return quotedArgs
}
