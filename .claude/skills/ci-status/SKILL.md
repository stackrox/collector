---
name: ci-status
description: Check CI status on current PR, fetch failure logs, diagnose issues
tags: [collector, ci, github, testing]
---

# CI Status

Check CI pipeline status for the current branch/PR and diagnose failures.

## Steps

1. Get the current branch and check for an open PR:
   ```bash
   gh pr view --json number,title,url,statusCheckRollup,headRefName
   ```
   If no PR exists, report that and suggest pushing or creating one.

2. Parse the check results. Group by status:
   - Passed checks
   - Failed checks (show full names)
   - Pending/running checks

3. For any **failed checks**:
   - Identify which workflow failed (unit-tests, integration-tests, k8s-integration-tests, benchmarks, lint)
   - Get the failed job logs:
     ```bash
     gh run view <run-id> --log-failed 2>&1 | tail -100
     ```
   - For integration test failures, identify:
     - Which VM type failed (rhel, ubuntu, cos, flatcar, etc.)
     - Which test suite failed (e.g., TestProcessNetwork, TestConnectionsAndEndpoints)
     - The relevant error message

4. **Diagnose** the failure:
   - If a unit test failed: show the failing assertion and the relevant source file
   - If an integration test failed: identify if it's a test infrastructure issue (VM creation, timeout) vs an actual test failure
   - If lint failed: show which files need formatting
   - If build failed: show the compiler error with file:line

5. **Suggest next steps**: what code changes would fix the failure, or if it's a flaky test / infra issue.
