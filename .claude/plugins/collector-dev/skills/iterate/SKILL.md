---
name: iterate
description: Full development cycle — build, unit test, format check, commit, push to existing branch
allowed-tools: Bash(cmake *), Bash(make *), Bash(ctest *), Bash(nproc), Bash(git *), Bash(clang-format *), Read, Write, Edit, Glob, Grep, mcp__github__pull_request_read, mcp__github__actions_list, mcp__github__actions_get, mcp__github__get_job_logs
---

# Iterate

Run the full development inner loop. The branch and PR already exist — just build, test, and push.
Stops at the first failure.

## Steps

1. **Build** the collector:
   - Detect environment (devcontainer vs host)
   - In devcontainer: `cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long) && cmake --build cmake-build -- -j$(nproc)`
   - On host: `make collector`
   - **Stop on failure** — report the compiler error with file:line.

2. **Unit test**:
   - In devcontainer: `ctest --no-tests=error -V --test-dir cmake-build`
   - On host: `make unittest`
   - **Stop on failure** — report which test failed and the assertion.

3. **Format check** (C++ files changed in this branch only):
   - Get changed C++ files: `git diff --name-only origin/master...HEAD | grep -E '\.(cpp|h)$'`
   - Run: `clang-format --style=file -n --Werror <files>`
   - If formatting issues found, auto-fix them: `clang-format --style=file -i <files>`
   - Report what was fixed.

4. **Commit**:
   - Stage changed files (source + any format fixes)
   - Create a commit with a descriptive message summarizing the changes

5. **Push**:
   - `git push` to the existing branch (branch and PR already created by run.sh)
   - Do NOT create new branches or PRs

6. **Check CI**:
   - Use `mcp__github__actions_list` to see if CI has started
   - Report the PR URL and note that CI is running
   - Use `/collector-dev:ci-status` for detailed CI results once checks complete
