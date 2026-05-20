#!/usr/bin/env python3
"""
Analyze test failures using Claude via GCP Vertex AI.

This script:
1. Collects test failure information from artifacts
2. Parses XML test reports and logs
3. Uses Claude (via Vertex AI) to analyze the failures
4. Generates a summary and recommendations
"""

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional

import anthropic
from google.auth import default
from google.auth.transport.requests import Request


def find_test_reports(artifacts_dir: Path) -> List[Path]:
    """Find all XML test report files."""
    reports = []
    if artifacts_dir.exists():
        reports.extend(artifacts_dir.glob("**/integration-test-report-*.xml"))
        reports.extend(artifacts_dir.glob("**/junit.xml"))
        reports.extend(artifacts_dir.glob("**/*test*.xml"))
    return reports


def parse_test_report(report_file: Path) -> Dict:
    """Parse JUnit XML test report."""
    try:
        tree = ET.parse(report_file)
        root = tree.getroot()

        failures = []
        errors = []

        # Handle both junit and testsuites root elements
        testcases = root.findall(".//testcase")

        for testcase in testcases:
            name = testcase.get("name", "Unknown")
            classname = testcase.get("classname", "")
            time = testcase.get("time", "0")

            failure = testcase.find("failure")
            error = testcase.find("error")

            if failure is not None:
                failures.append({
                    "test": f"{classname}.{name}" if classname else name,
                    "message": failure.get("message", ""),
                    "type": failure.get("type", ""),
                    "text": failure.text or "",
                    "time": time
                })

            if error is not None:
                errors.append({
                    "test": f"{classname}.{name}" if classname else name,
                    "message": error.get("message", ""),
                    "type": error.get("type", ""),
                    "text": error.text or "",
                    "time": time
                })

        return {
            "file": str(report_file),
            "failures": failures,
            "errors": errors,
            "total_failures": len(failures),
            "total_errors": len(errors)
        }
    except Exception as e:
        print(f"Error parsing {report_file}: {e}", file=sys.stderr)
        return {
            "file": str(report_file),
            "failures": [],
            "errors": [],
            "parse_error": str(e)
        }


def find_log_files(artifacts_dir: Path) -> List[Path]:
    """Find relevant log files."""
    logs = []
    if artifacts_dir.exists():
        logs.extend(artifacts_dir.glob("**/*.log"))
        logs.extend(artifacts_dir.glob("**/collector*.txt"))
    return logs[:10]  # Limit to 10 most recent logs


def get_claude_client() -> anthropic.AnthropicVertex:
    """Initialize Claude client with GCP Vertex AI authentication."""
    # Get GCP project and region from environment
    project_id = os.environ.get("GCP_PROJECT_ID", "rhacs-eng")
    region = os.environ.get("GCP_CLAUDE_REGION", "us-central1")

    # Authenticate with GCP
    credentials, project = default()
    if credentials.expired:
        credentials.refresh(Request())

    # Create Anthropic Vertex AI client
    client = anthropic.AnthropicVertex(
        project_id=project_id,
        region=region
    )

    return client


def analyze_failures_with_claude(
    test_reports: List[Dict],
    workflow_name: str,
    run_url: str
) -> Dict:
    """Use Claude to analyze test failures and provide insights."""

    # Prepare the failure summary
    total_failures = sum(r.get("total_failures", 0) for r in test_reports)
    total_errors = sum(r.get("total_errors", 0) for r in test_reports)

    if total_failures == 0 and total_errors == 0:
        return {
            "summary": "No test failures found in artifacts.",
            "details": "",
            "recommendations": []
        }

    # Build context for Claude
    failures_text = ""
    for report in test_reports:
        if report.get("failures") or report.get("errors"):
            failures_text += f"\n## Report: {Path(report['file']).name}\n"

            for failure in report.get("failures", []):
                failures_text += f"\n### Failed Test: {failure['test']}\n"
                failures_text += f"Type: {failure['type']}\n"
                failures_text += f"Message: {failure['message']}\n"
                if failure['text']:
                    failures_text += f"Details:\n```\n{failure['text'][:1000]}\n```\n"

            for error in report.get("errors", []):
                failures_text += f"\n### Error in Test: {error['test']}\n"
                failures_text += f"Type: {error['type']}\n"
                failures_text += f"Message: {error['message']}\n"
                if error['text']:
                    failures_text += f"Details:\n```\n{error['text'][:1000]}\n```\n"

    # Construct prompt for Claude
    prompt = f"""You are analyzing test failures from the StackRox Collector project, which is a runtime security data collection agent for Kubernetes/OpenShift.

Workflow: {workflow_name}
Total Failures: {total_failures}
Total Errors: {total_errors}

Test Failure Details:
{failures_text}

Please provide:
1. A concise summary (2-3 sentences) of the root cause or common pattern
2. Specific recommendations for fixing these failures
3. Any notable patterns or related issues you observe

Keep the analysis focused and actionable for the on-call engineer."""

    try:
        client = get_claude_client()

        message = client.messages.create(
            model="claude-3-5-sonnet@20240620",
            max_tokens=2000,
            messages=[
                {
                    "role": "user",
                    "content": prompt
                }
            ]
        )

        response_text = message.content[0].text

        # Parse response into sections
        lines = response_text.strip().split("\n")
        summary = []
        recommendations = []
        in_recommendations = False

        for line in lines:
            if "recommendation" in line.lower() or "fix" in line.lower():
                in_recommendations = True
            if in_recommendations:
                recommendations.append(line)
            else:
                summary.append(line)

        return {
            "summary": "\n".join(summary[:10]),  # First 10 lines as summary
            "details": response_text,
            "recommendations": recommendations,
            "total_failures": total_failures,
            "total_errors": total_errors
        }

    except Exception as e:
        print(f"Error calling Claude API: {e}", file=sys.stderr)
        return {
            "summary": f"Found {total_failures} failures and {total_errors} errors. Claude analysis failed: {str(e)}",
            "details": failures_text[:1000],
            "recommendations": ["Check the test artifacts manually for more details"],
            "total_failures": total_failures,
            "total_errors": total_errors,
            "error": str(e)
        }


def main():
    parser = argparse.ArgumentParser(description="Analyze test failures with Claude")
    parser.add_argument("--artifacts-dir", type=Path, required=True,
                       help="Directory containing test artifacts")
    parser.add_argument("--output-file", type=Path, required=True,
                       help="Output JSON file for analysis results")
    parser.add_argument("--workflow-name", type=str, required=True,
                       help="Name of the workflow that failed")
    parser.add_argument("--run-url", type=str, required=True,
                       help="URL to the workflow run")

    args = parser.parse_args()

    print(f"Searching for test reports in {args.artifacts_dir}")

    # Find and parse test reports
    report_files = find_test_reports(args.artifacts_dir)
    print(f"Found {len(report_files)} test report files")

    if not report_files:
        print("No test reports found, checking for any artifacts...")
        # List what we do have
        if args.artifacts_dir.exists():
            all_files = list(args.artifacts_dir.rglob("*"))
            print(f"Found {len(all_files)} files total")
            for f in all_files[:20]:
                print(f"  - {f}")

    test_reports = [parse_test_report(f) for f in report_files]

    # Analyze with Claude
    print("Analyzing failures with Claude...")
    analysis = analyze_failures_with_claude(
        test_reports,
        args.workflow_name,
        args.run_url
    )

    # Write output
    args.output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output_file, "w") as f:
        json.dump(analysis, f, indent=2)

    print(f"Analysis written to {args.output_file}")
    print("\n=== Analysis Summary ===")
    print(analysis.get("summary", "No summary available"))


if __name__ == "__main__":
    main()
