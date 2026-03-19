---
name: watch-ci
description: Push to existing remote branch via GitHub MCP, create PR if needed, monitor CI, fix failures until green
disable-model-invocation: true
allowed-tools: Bash(cmake *), Bash(ctest *), Bash(nproc), Bash(git add *), Bash(git commit *), Bash(git diff *), Bash(git describe *), Bash(git branch *), Bash(git status), Bash(git log *), Bash(git rev-parse *), Bash(clang-format *), Bash(sleep *), Bash(date *), Read, Write, Edit, Glob, Grep
---

# Watch CI

Push commits via the GitHub MCP server, create PR if needed, and monitor CI until green.
Do NOT use `git push` — it is blocked. Use the GitHub MCP push_files tool instead.

## Prerequisites

The current branch must already have a remote tracking branch. Check with:
`git rev-parse --abbrev-ref --symbolic-full-name @{u}`
If this fails, stop and report: "No remote branch. Push from host first."

## Steps

1. **Check remote branch exists**:
   - `git rev-parse --abbrev-ref --symbolic-full-name @{u}`
   - If this fails, stop

2. **Push** current commits:
   - Use the GitHub MCP server push_files tool to push committed changes
   - Do NOT use `git push`

3. **Find or create PR**:
   - Use the GitHub MCP server to search for an open PR for this branch
   - If no PR exists, create a draft PR via the GitHub MCP server

4. **Monitor CI loop** (repeat until all checks pass or blocked):
   - Wait 10 minutes: `sleep 600`
   - Use the GitHub MCP server to get PR check status and workflow runs
   - Evaluate:
     - **All checks passed** → update PR body, report success and stop
     - **Checks still running** → update PR body, report progress, continue loop
     - **Checks failed** →
       - Get job logs via the GitHub MCP server
       - Diagnose:
         - Build failure: read error, fix code
         - Unit test failure: read assertion, fix code
         - Lint failure: run `clang-format --style=file -i`
         - Integration test infra flake (VM timeout, network): report as flake, continue
         - Integration test real failure: analyze and fix code
       - If fixable: fix → build → test → commit → push via MCP → update PR body → continue loop
       - If not fixable: update PR body, report diagnosis and stop

5. **Update PR body** after each CI check with a status section at the bottom:
   - Use the GitHub MCP server to update the PR description
   - Append or replace a `## Agent Status` section with:
     ```
     ## Agent Status
     **Last updated:** <timestamp from `date -u +"%Y-%m-%d %H:%M UTC"`)>
     **Last commit:** <short sha from `git rev-parse --short HEAD`>
     **CI cycle:** N of 6
     **Status:** PENDING | PASSED | FIXED | FLAKE | BLOCKED
     **Details:** <one-line summary of what happened this cycle>
     ```
   - Keep the original PR description above the Agent Status section

6. **Safety limits**:
   - Maximum 6 CI cycles (about 3 hours of monitoring)
   - If exceeded, update PR body and stop

7. **Summary**: end with a status line:
   - `PASSED` — all checks green
   - `PENDING` — checks still running
   - `FIXED` — failure diagnosed and fix pushed
   - `FLAKE` — infra failure, not a code issue
   - `BLOCKED` — failure requires human intervention
