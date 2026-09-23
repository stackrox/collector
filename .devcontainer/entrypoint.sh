#!/usr/bin/env bash
# Ensure Claude Code directories exist (volumes may mount as empty)
mkdir -p /home/dev/.claude/debug /home/dev/.commandhistory

# Set defaults so Claude Code doesn't prompt on startup
claude config set --global theme "${CLAUDE_THEME:-dark}" 2> /dev/null || true
claude config set --global verbose false 2> /dev/null || true

# Register GitHub MCP server if token is available
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    if ! claude mcp add-json github \
        "$(jq -n --arg token "$GITHUB_TOKEN" \
            '{"type":"http","url":"https://api.githubcopilot.com/mcp","headers":{"Authorization":("Bearer " + $token),"X-MCP-Toolsets":"context,repos,pull_requests,issues,actions,git"}}')" \
        --scope user 2> /dev/null; then
        echo "WARNING: Failed to register GitHub MCP server" >&2
    fi
else
    echo "NOTE: GITHUB_TOKEN not set — GitHub MCP tools unavailable" >&2
fi

exec "$@"
