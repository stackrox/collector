#!/usr/bin/env bash
# Ensure Claude Code directories exist (volumes may mount as empty)
mkdir -p /home/dev/.claude/debug /home/dev/.commandhistory

# Register GitHub MCP server if token is available
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
  claude mcp add-json github \
    '{"type":"http","url":"https://api.githubcopilot.com/mcp","headers":{"Authorization":"Bearer '"$GITHUB_TOKEN"'"}}' \
    --scope user 2>/dev/null || true
fi

exec "$@"
