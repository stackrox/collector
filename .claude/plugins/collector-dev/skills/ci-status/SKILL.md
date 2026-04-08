---
name: ci-status
description: Check CI status on current PR, fetch failure logs, diagnose issues
allowed-tools: Bash(git branch *), Bash(git log *), mcp__github__search_pull_requests, mcp__github__pull_request_read, mcp__github__actions_list, mcp__github__actions_get, mcp__github__get_job_logs, Read
---

# CI Status

Check CI pipeline status for the current branch/PR and diagnose failures.

## Steps

1. Get the current branch name from git.

2. Use `mcp__github__search_pull_requests` to find an open PR for this branch
   in `stackrox/collector`.

3. If a PR exists, use `mcp__github__pull_request_read` to get its check status.

4. Use `mcp__github__actions_list` to get workflow runs for the branch.

5. For any **failed runs**:
   - Use `mcp__github__actions_get` to get the run details
   - Use `mcp__github__get_job_logs` to fetch failure logs
   - Identify which workflow failed (unit-tests, integration-tests, k8s-integration-tests, lint)
   - For integration test failures, identify which VM type and test suite failed

6. **Diagnose** the failure:
   - Unit test failure: show the failing assertion and relevant source file
   - Integration test failure: distinguish infra issues (VM creation, timeout) from test failures
   - Lint failure: show which files need formatting
   - Build failure: show the compiler error with file:line

7. **Suggest next steps**: what code changes would fix the failure, or note if it's flaky/infra.
