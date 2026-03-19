---
name: dev-loop
description: Full autonomous development loop — implement, build, test, commit, push, create PR, monitor CI, fix failures until green
disable-model-invocation: true
allowed-tools: Bash(cmake *), Bash(ctest *), Bash(nproc), Bash(git add *), Bash(git commit *), Bash(git diff *), Bash(git describe *), Bash(git branch *), Bash(git status), Bash(git log *), Bash(git rev-parse *), Bash(clang-format *), Bash(sleep *), Bash(date *), Read, Write, Edit, Glob, Grep, Agent
---

# Dev Loop

Complete a development task end-to-end: implement, build, test, push, create PR, monitor CI, fix failures.
Do NOT stop until CI is green or you are blocked.

## Phase 1: Implement

1. Read and understand the task from $ARGUMENTS
2. Explore relevant code
3. Implement the changes
4. Build: `cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long) && cmake --build cmake-build -- -j$(nproc)`
   - If build fails, fix and retry
5. Test: `ctest --no-tests=error -V --test-dir cmake-build`
   - If tests fail, fix and retry
6. Format: `clang-format --style=file -i <changed .cpp/.h files>`
7. Commit: `git add` changed files, `git commit` with a descriptive message

## Phase 2: Push and create PR

1. Check if remote branch exists: `git rev-parse --abbrev-ref --symbolic-full-name @{u}`
   - If no remote branch, stop and report: "No remote branch. Push from host first."
2. Push via the GitHub MCP server push_files tool (do NOT use `git push`)
3. Search for an open PR for this branch via GitHub MCP
4. If no PR exists, create a draft PR via GitHub MCP

## Phase 3: Monitor CI

Loop until all checks pass or blocked (max 6 cycles, ~3 hours):

1. Wait 10 minutes: `sleep 600`
2. Check CI status via GitHub MCP (PR checks, workflow runs)
3. Update PR body with an `## Agent Status` section:
   ```
   ## Agent Status
   **Last updated:** <`date -u +"%Y-%m-%d %H:%M UTC"`>
   **Last commit:** <`git rev-parse --short HEAD`>
   **CI cycle:** N of 6
   **Status:** PENDING | PASSED | FIXED | FLAKE | BLOCKED
   **Details:** <one-line summary>
   ```
4. Evaluate:
   - **All checks passed** → update PR body, report success, stop
   - **Still running** → continue loop
   - **Failed** →
     - Get job logs via GitHub MCP
     - Diagnose: build error, test assertion, lint, infra flake
     - If fixable: fix → build → test → commit → push via MCP → continue
     - If infra flake: note as FLAKE, continue
     - If not fixable: update PR body, report BLOCKED, stop

## Phase 4: Check PR comments

Before each CI cycle, check if there are new PR review comments via GitHub MCP.
If a reviewer left feedback:
- Address the feedback (edit code, fix issues)
- Build and test
- Commit and push via MCP
- Note in the Agent Status section what feedback was addressed

## Completion

Print summary:
```
STATUS: PASSED | BLOCKED | TIMEOUT
Branch: <branch>
PR: <url>
Cycles: N
Changes: <list of files modified>
```
