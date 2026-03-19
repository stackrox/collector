---
name: task
description: End-to-end autonomous workflow — implement a task, push, create PR, monitor CI, fix failures until green
disable-model-invocation: true
allowed-tools: Bash(cmake *), Bash(make *), Bash(ctest *), Bash(nproc), Bash(git *), Bash(clang-format *), Bash(sleep *), Read, Write, Edit, Glob, Grep, Agent
---

# Task

Complete a development task end-to-end: implement, build, test, push, create PR, and monitor CI until all checks pass.

## Input

The task description is provided via $ARGUMENTS or in the initial prompt context (branch name, task).

## Workflow

### Phase 1: Implement

1. Read and understand the task
2. Explore relevant code in the repository
3. Implement the changes
4. Build the collector:
   - In devcontainer: `cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long) && cmake --build cmake-build -- -j$(nproc)`
   - On host: `make collector`
   - If build fails, fix and retry
5. Run unit tests:
   - In devcontainer: `ctest --no-tests=error -V --test-dir cmake-build`
   - On host: `make unittest`
   - If tests fail, fix and retry
6. Format check:
   - `git diff --name-only origin/master...HEAD | grep -E '\.(cpp|h)$'` to find changed files
   - `clang-format --style=file -i <files>` to fix formatting
7. Commit and push:
   - `git add` the changed files
   - `git commit` with a descriptive message
   - `git push -u origin HEAD`
8. Create a draft PR:
   - Use the GitHub MCP server to create a draft PR in stackrox/collector
   - Title: brief summary of the task
   - Body: describe what was changed and why

### Phase 2: Monitor CI

After pushing, enter a monitoring loop. CI typically takes 30-90 minutes.

**Loop** (repeat until all checks pass or blocked):

1. Wait 10 minutes: `sleep 600`
2. Check CI status:
   - Get current branch: `git branch --show-current`
   - Use the GitHub MCP server to search for the PR and get workflow run status

3. Evaluate:

   **All checks passed** → report success and stop

   **Checks still running** → report progress ("X of Y complete"), continue loop

   **Checks failed** →
   - Use the GitHub MCP server to get job logs for the failed run
   - Diagnose the failure:
     - Build failure: read error, fix code
     - Unit test failure: read assertion, fix code
     - Lint failure: run clang-format
     - Integration test infra flake (VM timeout, network): report as flake, continue loop
     - Integration test real failure: analyze and fix code
   - If fixable: fix → build → unit test → commit → push → continue loop
   - If not fixable: report diagnosis and stop

4. Safety limits:
   - Maximum 6 CI cycles (about 3 hours of monitoring)
   - If exceeded, report status and stop

### Completion

End with a summary:
```
STATUS: PASSED | BLOCKED | TIMEOUT
Branch: claude/agent-xxx
PR: <url>
Cycles: N
Changes: list of files modified
```
