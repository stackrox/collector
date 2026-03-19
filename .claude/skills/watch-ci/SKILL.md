---
name: watch-ci
description: Check CI status and react to failures — diagnose, fix, rebuild, push. Designed to run in a loop.
allowed-tools: Bash(cmake *), Bash(make *), Bash(ctest *), Bash(nproc), Bash(git *), Bash(clang-format *), Read, Write, Edit, Glob, Grep
---

# Watch CI

Monitor CI for the current branch's PR and react to failures. Designed to be run
with `/loop 30m /collector-dev:watch-ci`.

## Steps

1. **Find the PR** for the current branch:
   - Get branch name: `git branch --show-current`
   - Use the GitHub MCP server to search for the open PR in stackrox/collector
   - If no PR found, report and stop

2. **Check CI status**:
   - Use the GitHub MCP server to get PR check status and workflow runs

3. **Evaluate state and act**:

   **If all checks pass:**
   - Report: "All CI checks passed. PR is ready for review."
   - Stop — no further action needed

   **If checks are still running:**
   - Report: "CI still running (X of Y checks complete). Will check again next loop."
   - Stop — wait for next loop iteration

   **If checks failed:**
   - Use the GitHub MCP server to get job logs for the failed run
   - Identify the failure type:
     - **Build failure**: read compiler error, find the file:line, fix the code
     - **Unit test failure**: read the assertion, find the test and source, fix the code
     - **Integration test failure**: determine if it's a real failure or infra flake
       - If infra flake (VM creation timeout, network issue): report and skip
       - If real test failure: analyze the test expectation vs actual, fix the code
     - **Lint failure**: run `clang-format --style=file -i` on the affected files
   - After fixing:
     - Build: `cmake --build cmake-build -- -j$(nproc)`
     - Unit test: `ctest --no-tests=error -V --test-dir cmake-build`
     - If build+test pass: `git add`, `git commit`, `git push`
     - Report what was fixed and that a new CI run should start
   - If the failure can't be fixed automatically, report the diagnosis and stop

4. **Summary**: always end with a clear status line:
   - `PASSED` — all checks green
   - `PENDING` — checks still running, will retry
   - `FIXED` — failure diagnosed and fix pushed, awaiting new CI run
   - `FLAKE` — infra failure, not a code issue
   - `BLOCKED` — failure requires human intervention
