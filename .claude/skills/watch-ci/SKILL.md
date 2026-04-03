---
name: watch-ci
description: Push files to existing remote branch via GitHub MCP, create PR if needed, monitor CI, fix failures until green
disable-model-invocation: true
allowed-tools: Bash(cmake *), Bash(ctest *), Bash(nproc), Bash(git add *), Bash(git commit *), Bash(git diff *), Bash(git describe *), Bash(git branch *), Bash(git status), Bash(git log *), Bash(git rev-parse *), Bash(clang-format *), Bash(sleep *), Bash(date *), Read, Write, Edit, Glob, Grep
---

# Watch CI

Push changed files via the GitHub MCP server, create PR if needed, and monitor CI until green.
Do NOT use `git push` — it will fail (no SSH keys in this container).

## How pushing works

Use the GitHub MCP `push_files` tool to send file contents directly to the remote
branch via the GitHub API. This does NOT sync local git history — it creates a new
commit on the remote with the file contents you provide.

1. Get the branch name: `git branch --show-current`
2. Get changed files: `git diff --name-only origin/HEAD..HEAD`
3. Read each changed file's content
4. Call `push_files` with owner: stackrox, repo: collector, branch, files, and commit message

## Steps

1. **Push** changed files:
   - Use the GitHub MCP `push_files` tool as described above
   - If no files have changed since last push, skip

2. **Find or create PR**:
   - Use the GitHub MCP server to search for an open PR for this branch
   - If no PR exists, create a draft PR via the GitHub MCP server

3. **Monitor CI loop** (repeat until all checks pass or blocked):
   - Wait 10 minutes: `sleep 600`
   - Use the GitHub MCP server to get PR check status and workflow runs
   - Update PR body with an `## Agent Status` section:
     ```
     ## Agent Status
     **Last updated:** <`date -u +"%Y-%m-%d %H:%M UTC"`>
     **CI cycle:** N of 6
     **Status:** PENDING | PASSED | FIXED | FLAKE | BLOCKED
     **Details:** <one-line summary>
     ```
   - Evaluate:
     - **All checks passed** → update PR body, report success and stop
     - **Checks still running** → report progress, continue loop
     - **Checks failed** →
       - Get job logs via the GitHub MCP server
       - Diagnose:
         - Build failure: read error, fix code
         - Unit test failure: read assertion, fix code
         - Lint failure: run `clang-format --style=file -i`
         - Integration test infra flake (VM timeout, network): report as flake, continue
         - Integration test real failure: analyze and fix code
       - If fixable: fix → build → test → push changed files via MCP → continue loop
       - If not fixable: update PR body, report diagnosis and stop

4. **Safety limits**:
   - Maximum 6 CI cycles (about 3 hours of monitoring)
   - If exceeded, update PR body and stop

5. **Summary**: end with a status line:
   - `PASSED` — all checks green
   - `PENDING` — checks still running
   - `FIXED` — failure diagnosed and fix pushed
   - `FLAKE` — infra failure, not a code issue
   - `BLOCKED` — failure requires human intervention
