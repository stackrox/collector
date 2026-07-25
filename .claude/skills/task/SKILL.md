---
name: task
description: Implement a change — edit code, build, test, format, commit locally. No push.
disable-model-invocation: true
allowed-tools: Bash(cmake *), Bash(ctest *), Bash(nproc), Bash(git add *), Bash(git commit *), Bash(git diff *), Bash(git describe *), Bash(git branch *), Bash(git status), Bash(clang-format *), Read, Write, Edit, Glob, Grep, Agent
---

# Task

Implement a change locally: edit, build, test, format, commit.
Do NOT push or create PRs — use /watch-ci for that.

## Steps

1. Read and understand the task from $ARGUMENTS
2. Explore relevant code in the repository
3. Implement the changes
4. Build:
   - `cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCOLLECTOR_VERSION=$(git describe --tags --abbrev=10 --long) && cmake --build cmake-build -- -j$(nproc)`
   - If build fails, fix and retry
5. Run unit tests:
   - `ctest --no-tests=error -V --test-dir cmake-build`
   - If tests fail, fix and retry
6. Format changed C++ files:
   - `clang-format --style=file -i <changed .cpp/.h files>`
7. Commit:
   - `git add` the changed files
   - `git commit` with a descriptive message

## STOP here. Report and wait.

Print this summary and then STOP. Do not continue with any other actions.

```
TASK COMPLETE
Branch: <current branch>
Commit: <commit hash>
Files changed: <list>
Tests: <pass/fail count>
```

The user will review and decide whether to run /watch-ci.
Do NOT push, create branches, or create PRs.
