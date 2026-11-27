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

Use the GitHub MCP server to push files and create a PR.
Do NOT use `git push` — it will fail (no SSH keys in this container).

1. Get the current branch name and the list of changed files:
   - `git branch --show-current` for the branch
   - `git diff --name-only origin/HEAD..HEAD` for changed files
2. Use the GitHub MCP `push_files` tool to push the changed files directly to
   the remote branch. This creates a commit via the GitHub API using the file
   contents from your local workspace — it does not sync git history.
   - owner: stackrox, repo: collector, branch: <current branch>
   - Read each changed file and include its content
   - Provide a commit message
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
     - If fixable: fix → build → test → push changed files via MCP → continue
     - If infra flake: note as FLAKE, continue
     - If not fixable: update PR body, report BLOCKED, stop

## Phase 4: Check PR comments

Before each CI cycle, check if there are new PR review comments via GitHub MCP.
If a reviewer left feedback:
- Address the feedback (edit code, fix issues)
- Build and test
- Push changed files via MCP
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
