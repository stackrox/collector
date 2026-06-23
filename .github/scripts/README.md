# Test Failure Analysis with Claude

Automatically analyzes test failures using Claude AI and includes intelligent insights in Slack notifications.

## Architecture

```
Integration Tests Run
  ├── amd64-integration-tests (may fail)
  ├── arm64-integration-tests (may fail)
  ├── s390x-integration-tests (may fail)
  ├── ppc64le-integration-tests (may fail)
  │
  ├── collect-failures
  │    └── Determine which jobs failed
  │
  └── analyze-and-notify (reusable workflow)
       ├── analyze-failures
       │    ├── Download test artifacts
       │    ├── Execute /analyze-test-failures skill
       │    └── Upload analysis-report.md
       │
       └── notify
            ├── Download analysis-report.md
            └── Post to Slack with AI insights
```

## How It Works

### 1. Test Failures
Any integration test job fails (e.g., `rhcos-arm64`, `cos-logs`)

### 2. Collect Failures
The `collect-failures` job identifies which jobs failed and outputs the list

### 3. Analyze Failures (Claude Skill)
Uses `claude-code-base-action` to execute the `/analyze-test-failures` skill:

**The skill (`.claude/commands/analyze-test-failures.md`):**
- Finds and parses JUnit XML test reports
- Reads failing test source code
- Examines implementation code being tested
- Checks git log for recent changes
- Identifies platform-specific patterns (arch/OS)
- Creates `analysis-report.md` with actionable insights

**Claude has access to:**
- `Skill` - Load and execute the analysis skill
- `Read` - View source files
- `Grep` - Search codebase
- `Glob` - Find files
- `Bash` - Execute git commands, create reports

### 4. Notify
Posts to Slack (#team-acs-collector-oncall) with:
- AI-generated root cause analysis
- Evidence from code and logs
- Platform-specific patterns detected
- Actionable recommendations with file:line references

Falls back to simple notification if analysis fails.

## Files

### Workflows
- `.github/workflows/integration-tests.yml` - Main integration test workflow
- `.github/workflows/analyze-and-notify.yml` - Reusable analysis workflow

### Skill
- `.claude/commands/analyze-test-failures.md` - Claude skill defining analysis logic

## Example Output

**Slack message with AI analysis:**
```
@acs-collector-oncall

🤖 AI Analysis

**Root Cause**: NetworkSignalHandler.cpp:245 missing ntohs() call 
causing UDP checksum failures on ARM64 platforms.

**Evidence**:
• UDP test failures isolated to arm64 runners (rhcos-arm64, cos-arm64)
• Checksum comparison uses direct equality without byte order conversion
• Recent commit abc123f modified network packet handling
• Tests pass on amd64 where byte order matches

**Affected Platforms**: arm64 (rhcos-arm64, cos-arm64, ubuntu-arm)

**Recommendations**:
• Fix collector/lib/NetworkSignalHandler.cpp:245 - add ntohs() call
• Add endianness test to integration suite
• Review other protocol handlers for similar issues

---
**Statistics**
• Total Failures: 2
• Failed Jobs: rhcos-arm64, cos-arm64
```

## How It's Different from Manual Analysis

**Before:** Generic notification
```
@acs-collector-oncall
Integration tests failed.
```

**After:** Actionable analysis with Claude
- Specific file and line number to fix
- Root cause explanation based on code analysis
- Platform/architecture pattern detection
- Links recent git changes to failures
- Provides concrete next steps

## Testing

### Test on a PR

Add the label `test-oncall-workflow` to any PR to trigger the workflow.

**What happens:**
- Workflow runs with empty test artifacts
- Claude analyzes and generates a report
- Report is uploaded as artifact
- **Slack notification is skipped** (only runs on actual test failures)

**Use case:** Verify Claude analysis executes without spamming Slack.

**To verify it worked:**
1. Check the workflow run in Actions tab
2. Download the `failure-analysis` artifact to see the generated report

### Test with Real Failures

The best test is observing the workflow on actual test failures:
1. Wait for integration tests to fail naturally
2. Check #team-acs-collector-oncall for the AI analysis
3. Verify the analysis is helpful and actionable

## Configuration

### Vertex AI Region
Set in `.github/workflows/analyze-and-notify.yml`:
```yaml
env:
  CLOUD_ML_REGION: us-east5
```

### Required Secrets

Already configured:
- `GCP_CLAUDE_SERVICE_ACCOUNT_KEY` - Service account JSON for Vertex AI
- `GCP_CLAUDE_PROJECT_ID` - GCP project ID
- `SLACK_COLLECTOR_ONCALL_WEBHOOK` - Slack webhook URL

### Allowed Tools

Claude has access to these tools for investigation:
```yaml
allowed_tools: "Skill,Read,Grep,Glob,Bash"
```

### Reusable Workflow Inputs

The `analyze-and-notify.yml` workflow accepts:
- `failed-jobs` - Comma-separated list of failed job names
- `workflow-name` - Name of the workflow that failed

## Troubleshooting

### No Analysis Report Generated

**Check:**
1. Claude action step logs - did it execute successfully?
2. "Check if analysis report was created" step - does file exist?
3. Skill file exists at `.claude/commands/analyze-test-failures.md`
4. `Skill` tool is in `allowed_tools`

### Vertex AI Errors

**Common issues:**
- Model not available in configured region
- Service account lacks `roles/aiplatform.user` permission
- `GCP_CLAUDE_PROJECT_ID` secret not set correctly

**Solution:**
Check Claude action logs for specific error details.

### No Slack Notification

**Check:**
1. `SLACK_COLLECTOR_ONCALL_WEBHOOK` secret is set
2. Notify job logs show download step succeeded
3. Webhook URL is valid

### Analysis Quality Issues

**If Claude's analysis is not helpful:**
1. Check that test artifacts are being uploaded correctly
2. Verify JUnit XML format is valid
3. Update skill instructions in `.claude/commands/analyze-test-failures.md`
4. The skill can be iterated on independently of the workflow

## Local Development

### Test the Skill Locally

```bash
# Requires Claude CLI installed
claude /analyze-test-failures test-artifacts/ "Integration Tests" "rhcos-arm64,cos"
```

### Update the Skill

Edit `.claude/commands/analyze-test-failures.md` to:
- Change analysis instructions
- Update report format
- Add new investigation steps
- Modify recommendations structure

Changes take effect on the next workflow run - no workflow YAML changes needed.

## Future Enhancements

- [ ] Correlate failures with specific PR/commit
- [ ] Track failure patterns over time  
- [ ] Link to similar historical failures
- [ ] Auto-create issues for recurring failures
- [ ] Support for other test frameworks beyond JUnit XML
- [ ] Integration with test retries/flakiness detection
