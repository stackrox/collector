---
name: ci-report
description: Generate a CI oncall handoff report analyzing GitHub Actions workflow runs on master and release branches. Shows failures, flaky tests, and action items.
user_invocable: true
---

# CI Oncall Report

Generate a concise CI health report for the Collector oncall handler. This report is designed for oncall handoff — lead with action items, keep it tight.

## Arguments

The user may provide a natural language time range as the argument (e.g. "today", "last 3 days", "this week", "since Monday"). Default to "last 24 hours" if no argument is given. Cap at 7 days maximum — if the user requests more, tell them and use 7 days.

Convert the time range to an ISO 8601 date (YYYY-MM-DD) for the `--created` filter. Use `>=` (not `>`) so that "today" includes today's runs.

## Process

Follow these steps in order. Use the Bash tool for all `gh` commands.

### Step 1: Detect the repository

```bash
gh repo view --json nameWithOwner -q .nameWithOwner
```

If this fails, fall back to parsing the git remote:
```bash
git remote get-url origin | sed 's|.*github.com[:/]||;s|\.git$||'
```

Store the result as `REPO` for subsequent commands.

### Step 2: Fetch all workflow runs

```bash
gh run list --repo REPO --created ">=YYYY-MM-DD" --limit 500 --json headBranch,status,conclusion,workflowName,databaseId,createdAt,updatedAt,url,event
```

If exactly 500 results are returned, warn the user that results may be truncated and suggest narrowing the time window.

Filter the results to only branches matching `^(master|release-\d+\.\d+)$` — this excludes feature branches and sub-branches like `release-3.24/foo`.

### Step 3: Group and summarize

Count **workflow runs** (not individual jobs within runs). Each entry from `gh run list` is one workflow run. For each branch, count:
- Passed (conclusion == "success")
- Failed (conclusion == "failure")
- Cancelled (reported separately, excluded from pass rate)

Do NOT count skipped jobs or individual job statuses in the summary table — this table is about whole workflow runs only. Individual job failures belong in the Failure Details section.

Calculate pass rate as: passed / (passed + failed) * 100.

### Step 4: Fetch failure details

For each failed run:

1. Get the failed jobs:
```bash
gh run view RUN_ID --repo REPO --json jobs
```

2. Get the failed log output and search for the **real root cause**:
```bash
gh run view RUN_ID --repo REPO --log-failed 2>&1 | grep -e "FAIL:" -e "fatal:.*FAILED" -e "TASK \[" -e "Configure VSI" -e "Run integration tests" -e "##\[error\]" | grep -v "RETRYING" | head -20
```

**IMPORTANT: Do NOT use `tail` on the log output.** The end of the log is typically git cleanup, artifact upload, and `Unarchive logs` steps — these are post-test housekeeping, not the root cause. The `Unarchive logs` step failing with `tar: container-logs/*.tar.gz: Cannot open` is a **symptom** (tests didn't produce logs), never the root cause.

Instead, search the full log for the actual failure by looking for:
- `--- FAIL: TestName` — Go test failures (the tests ran and a specific test failed)
- `fatal: [hostname]: FAILED!` — Ansible task failures (VM provisioning, image pulls, etc.)
- `TASK [task-name]` lines immediately before `fatal:` lines — identifies which ansible step failed
- `##[error]` — GitHub Actions step errors
- Build/compilation errors

3. If you need more context around a specific error, use:
```bash
gh run view RUN_ID --repo REPO --log --job JOB_ID 2>&1 | grep -B5 -A10 "FAIL:\|fatal:.*FAILED" | head -50
```

4. Classify the root cause:
- **Test failure**: A `--- FAIL: TestName` line means the tests ran and failed. Report the test name and the assertion/error message. These are real regressions or flaky tests.
- **VM provisioning failure**: An ansible `fatal:` on a `create-vm` or `Configure VSI` task means the test environment couldn't be set up. This is infrastructure, not a test problem.
- **Image pull failure**: An ansible `fatal:` on `Pull non-QA images` or `Pull QA images` could be a non-fatal warning if the tests still ran afterwards. Check whether `Run integration tests` appears later in the log — if it does, the pull failure was not the root cause.
- **Build failure**: A compilation error in the build step. Report the file and error.

5. Summarize the root cause in one line, naming the specific test or ansible task that failed.

If log fetching fails for a specific run, note it and continue with other runs.

### Step 5: Detect flakiness

Compare runs of the same workflow on the same branch. A job is flaky if it fails in some runs but passes in others within the time window **and the failure has the same root cause each time**. Track the failure frequency (e.g. "failed 2/5 runs").

A job that fails in multiple runs with **different** root causes (e.g. one run hits a repo mirror issue, another hits a timeout) is NOT flaky — those are separate infrastructure problems. Only flag as flaky when the same failure pattern repeats intermittently.

### Step 6: Check previous reports for trends

Look for existing report files in `docs/oncall/`:
```bash
ls -1 docs/oncall/*-ci-report*.md 2>/dev/null | sort -r | head -5
```

If previous reports exist, read them and extract:
- **Pass rate per branch** from their Branch Health Summary tables
- **Action items** from their Action Items sections

Use this to build two trend views:
1. **Pass rate trends** — how each branch's pass rate has changed across reports
2. **Action item tracking** — which items from previous reports are now resolved vs still failing

If no previous reports exist, skip the trends section in the output.

### Step 7: Generate the report

Write the report following this exact structure. Be concise throughout — the report should be readable in under 2 minutes.

**Linking**: Every claim in the report must be independently verifiable. Use the `url` field from the `gh run list` output to link to specific workflow runs. The GitHub Actions filter URL for a branch is `https://github.com/REPO/actions?query=branch%3aBRANCH_NAME`. Include these links so a human reader can click through and verify any data point.

#### Section 1: Action Items

This is the most important section. Put it first. List things needing attention, most urgent first. Each item should include:
- What needs attention and why
- Link to the relevant run(s)
- Classification: regression, flaky, infrastructure, or needs investigation

Example format:
```
- **Regression**: integration-tests failing on master since Mar 24 — NetworkConnection test timeout. [Run #1234](url)
- **Flaky**: k8s-integration-tests on release-3.24 — fails 2/5 runs, ProcessSignal assertion. [Run #1230](url)
- **Investigate**: Konflux build failures on release-3.23 — image pull error. [Run #1228](url)
```

If nothing needs attention: "All clear — no action items."

#### Section 2: Branch Health Summary

One line per branch. Count whole workflow runs only (not individual jobs). Cancelled runs shown separately, excluded from pass rate. Do NOT add a "Skipped" column. Link each branch name to its GitHub Actions filter page.

```
| Branch       | Runs | Passed | Failed | Cancelled | Pass Rate |
|--------------|------|--------|--------|-----------|-----------|
| [master](https://github.com/REPO/actions?query=branch%3Amaster) | 12 | 11 | 1 | 0 | 92% |
| [release-3.24](https://github.com/REPO/actions?query=branch%3Arelease-3.24) | 8 | 6 | 2 | 0 | 75% |
```

#### Section 3: Flaky Jobs

Only include this section if flakiness was detected. Link to an example failing run for each entry.

```
| Job               | Branch       | Fail Rate | Pattern            | Example |
|-------------------|--------------|-----------|---------------------|---------|
| NetworkConnection | master       | 2/10      | Timeout after 120s  | [Run #1234](url) |
```

#### Section 4: Failure Details

Group by root cause where possible. Each entry:
- Branch, workflow, run link
- Failed job name
- One-line root cause

Only include log excerpts when the cause is non-obvious. Keep this section short.

#### Section 5: Trends

Only include this section if previous reports were found in `docs/oncall/`.

**Pass rate trends** — show how each branch's health has changed. Use the dates from previous report filenames as column headers.

```
| Branch       | Mar 23 | Mar 24 | Mar 25 (today) |
|--------------|--------|--------|----------------|
| master       | 100%   | 85%    | 92%            |
| release-3.24 | 90%    | 75%    | 75%            |
```

**Action item tracking** — compare today's action items against previous reports. For each previous action item, note whether it's resolved, still present, or new.

```
- **Resolved**: NetworkConnection timeout on master (first seen Mar 23, resolved today)
- **Ongoing**: Konflux build failures on release-3.23 (first seen Mar 24, still failing)
- **New**: integration-tests regression on master (first seen today)
```

Keep this concise — only mention items that changed status or have persisted for multiple reports.

#### Section 6: Stats

Reference information at the bottom:
- Date range analyzed
- Total runs across all branches
- Overall pass rate
- Report generated timestamp

### Step 8: Save the report

1. Create the output directory if needed:
```bash
mkdir -p docs/oncall
```

2. Save to `docs/oncall/YYYY-MM-DD-ci-report.md` using today's date.

3. If that file already exists, try `-2`, `-3`, etc. until a unique filename is found.

4. Display the full report content in the terminal as well.

## Error Handling

- If `gh` is not authenticated, tell the user to run `! gh auth login` (the `!` prefix runs it in the current session).
- If no runs are found in the time window, report that clearly — don't generate an empty report.
- If individual log fetches fail, note the failure and continue with other runs.
