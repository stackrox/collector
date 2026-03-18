---
name: iterate
description: Full development cycle — build, unit test, format check, commit, push
tags: [collector, build, test, workflow]
---

# Iterate

Run the full development inner loop. Stops at the first failure.

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
   - Follow the repository's commit conventions

5. **Push**:
   - Push to the current branch
   - If a PR exists, report the PR URL
   - If no PR exists, suggest creating one

6. **Report**:
   - Summary: built, N tests passed, formatted M files, pushed to branch X
   - Link to PR if it exists
   - Note: CI will run integration tests on the PR — use `/ci-status` to check results later
