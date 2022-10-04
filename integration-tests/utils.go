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

// Generate the QA tag to be used for containers by attaching the contents of
// the 'COLLECTOR_QA_TAG' environment variable if it exists. Return the base
// tag as is otherwise.
func getQATag(base_tag string) string {
	collector_qa_tag := ReadEnvVar("COLLECTOR_QA_TAG")

	if collector_qa_tag != "" {
		return base_tag + "-" + collector_qa_tag;
	}
	return base_tag
}

// Return the full image to be used for a QA container from a given image name
// and a tag. The tag will be adjusted accordingly to the description of
// 'getQaTag'
func qaImage(image string, tag string) string {
	return image + ":" + getQATag(tag);
}
