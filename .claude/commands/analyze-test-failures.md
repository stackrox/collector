---
name: analyze-test-failures
description: Analyze test failure artifacts and generate root cause analysis report
---

# Test Failure Analysis

Analyze test failures from CI artifacts and generate a concise root cause analysis for the oncall team.

## Usage

```
/analyze-test-failures <artifacts-dir> <workflow-name> <failed-jobs>
```

**Arguments:**
- `artifacts-dir`: Directory containing test artifacts (default: test-artifacts/)
- `workflow-name`: Name of the workflow that failed (e.g., "Integration Tests")
- `failed-jobs`: Comma-separated list of failed job names

**Example:**
```
/analyze-test-failures test-artifacts/ "Integration Tests" "amd64-integration-tests,arm64-integration-tests"
```

## What This Does

1. **Find test reports**: Searches for JUnit XML files (integration-test-report-*.xml, junit.xml)
2. **Parse failures**: Extracts test names, error messages, stack traces
3. **Investigate code**: Reads failing test source and implementation code
4. **Check git history**: Looks for recent changes that may have caused failures
5. **Identify patterns**: Detects platform-specific issues (arch/OS)
6. **Generate report**: Creates analysis-report.md with findings

## Report Format

The generated `analysis-report.md` contains:

```markdown
**🤖 AI Analysis**

**Root Cause**: [1-2 sentence summary with file:line references]

**Evidence**:
• [Specific code observations]
• [Patterns across failures]
• [Recent changes correlation]

**Affected Platforms**: [Architectures/OS if pattern found]

**Recommendations**:
• [Specific file:line to fix with suggested change]
• [Additional investigation needed]
• [Prevention strategy]

---
**Statistics**
• Total Failures: [count]
• Total Errors: [count]
• Failed Jobs: [list]
```

## Implementation

Start by finding and parsing test reports:

```bash
# Find all XML test reports
find <artifacts-dir> -name "*.xml" -type f
```

For each failure:
- Read the test source code to understand intent
- Examine the implementation being tested
- Check `git log --oneline -20` for recent changes
- Look for patterns across different platforms

Generate the report focusing on **actionable insights** for the oncall engineer:
- File paths and line numbers for fixes
- Platform-specific patterns (endianness, timing, etc.)
- Links to similar past failures if found

Keep the analysis **under 500 words** and emphasize:
- What broke
- Why it broke
- How to fix it

## CRITICAL: File Creation Step

You MUST execute this bash command to create the report file:

```bash
cat > analysis-report.md <<'EOF'
**🤖 AI Analysis**

**Root Cause**: [your analysis here]

**Evidence**:
• [your findings]

**Affected Platforms**: [platforms]

**Recommendations**:
• [actionable fixes]

---
**Statistics**
• Total Failures: [count]
• Failed Jobs: [jobs]
EOF
```

DO NOT just summarize your findings - you MUST create the actual file using the bash command above.

This is a required step. The workflow depends on analysis-report.md existing.
