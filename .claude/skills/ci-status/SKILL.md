---
name: ci-status
description: Check CI status on current PR, fetch failure logs, diagnose issues
allowed-tools: Bash(git branch *), Bash(git log *), Read
---

# CI Status

Check CI pipeline status for the current branch/PR and diagnose failures.

## Steps

1. Get the current branch name from git.

2. Use the GitHub MCP server to search for an open PR for this branch
   in stackrox/collector.

3. If a PR exists, get its check status via the GitHub MCP server.

4. Get workflow runs for the branch via the GitHub MCP server.

5. For any **failed runs**:
   - Get the run details and job logs via the GitHub MCP server
   - Identify which workflow failed (unit-tests, integration-tests, k8s-integration-tests, lint)
   - For integration test failures, identify which VM type and test suite failed

6. **Diagnose** the failure:
   - Unit test failure: show the failing assertion and relevant source file
   - Integration test failure: distinguish infra issues (VM creation, timeout) from test failures
   - Lint failure: show which files need formatting
   - Build failure: show the compiler error with file:line

7. **Suggest next steps**: what code changes would fix the failure, or note if it's flaky/infra.
