# Test Failure Analysis with Claude

This workflow automatically analyzes test failures using Claude AI (via GCP Vertex AI) and posts intelligent summaries to Slack.

## How It Works

1. **Trigger**: Runs automatically when integration tests or unit tests fail (on main/master, not PRs)
2. **Analysis**: 
   - Downloads test artifacts (XML reports, logs)
   - Parses JUnit XML test reports
   - Sends failure details to Claude for analysis
   - Claude identifies patterns, root causes, and provides recommendations
3. **Notification**: Posts analysis to Slack with:
   - Summary of failures
   - AI-generated insights
   - Recommendations for fixing
   - Links to full workflow run

## Required Secrets

Configure these in the repository secrets:

### Existing Secrets (already configured)
- `SLACK_COLLECTOR_ONCALL_WEBHOOK` - Slack webhook URL

### New Secrets Required
- `GCP_CLAUDE_SERVICE_ACCOUNT_KEY` - GCP service account JSON key with Vertex AI access
- `GCP_CLAUDE_PROJECT_ID` - GCP project ID (e.g., "rhacs-eng")
- `GCP_CLAUDE_REGION` - GCP region for Vertex AI (default: "us-central1")

### Setting up GCP Service Account

1. **Create Service Account**:
   ```bash
   gcloud iam service-accounts create claude-test-analyzer \
     --display-name="Claude Test Failure Analyzer"
   ```

2. **Grant Vertex AI Access**:
   ```bash
   gcloud projects add-iam-policy-binding YOUR_PROJECT_ID \
     --member="serviceAccount:claude-test-analyzer@YOUR_PROJECT_ID.iam.gserviceaccount.com" \
     --role="roles/aiplatform.user"
   ```

3. **Create and Download Key**:
   ```bash
   gcloud iam service-accounts keys create key.json \
     --iam-account=claude-test-analyzer@YOUR_PROJECT_ID.iam.gserviceaccount.com
   ```

4. **Add to GitHub Secrets**:
   - Go to repository Settings → Secrets and variables → Actions
   - Add `GCP_CLAUDE_SERVICE_ACCOUNT_KEY` with contents of `key.json`
   - Add `GCP_CLAUDE_PROJECT_ID` with your GCP project ID

## Example Slack Message

```
🤖 Collector Integration Tests - AI Analysis

Workflow: Collector Integration Tests
Status: Failed
Run: View Details

🤖 Claude Analysis:
The test failures appear to be related to eBPF program loading issues on 
RHCOS nodes. Three tests failed with "permission denied" errors when 
attempting to attach to the lsm/file_open hook.

Recommendations:
• Check kernel version on RHCOS test VMs - LSM BPF requires 5.7+
• Verify BPF LSM is enabled in kernel config
• Review recent changes to eBPF program loading logic
• Check for SELinux policy changes that might block BPF operations
```

## Testing Locally

```bash
# Set up environment
export GCP_PROJECT_ID="rhacs-eng"
export GCP_CLAUDE_REGION="us-central1"
export GOOGLE_APPLICATION_CREDENTIALS="/path/to/key.json"

# Install dependencies
pip install -r requirements.txt

# Run analysis
python analyze_test_failures.py \
  --artifacts-dir /path/to/test-artifacts \
  --output-file /tmp/analysis.json \
  --workflow-name "Test Run" \
  --run-url "https://github.com/..."
```

## Customization

### Adjusting Claude's Analysis

Edit the prompt in `analyze_test_failures.py` (line ~180) to:
- Focus on specific types of failures
- Include more context about the codebase
- Change the tone or detail level
- Add specific debugging steps

### Changing Slack Message Format

Edit the `SLACK_MESSAGE` in `.github/workflows/test-failure-analysis.yml` to:
- Add more fields
- Change formatting
- Include additional metrics
- Add custom emojis or mentions

## Workflow File

The workflow is defined in `.github/workflows/test-failure-analysis.yml` and runs as a `workflow_run` event, which triggers after the test workflows complete.

## Troubleshooting

**No analysis generated**:
- Check that test artifacts contain XML reports
- Verify GCP credentials are valid
- Check workflow logs for Python errors

**Claude API errors**:
- Verify service account has `aiplatform.user` role
- Check GCP project ID and region are correct
- Ensure Vertex AI API is enabled in GCP project

**Slack notification not posting**:
- Verify `SLACK_COLLECTOR_ONCALL_WEBHOOK` secret is set
- Check Slack webhook is still valid
- Review workflow logs for Slack API errors
