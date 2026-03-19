# MCP Context Protector Integration Plan

Trail of Bits [MCP Context Protector](https://github.com/trailofbits/mcp-context-protector)
as a security proxy between Claude Code and the GitHub MCP server.

## Architecture

```
Claude Code → MCP Context Protector (stdio) → GitHub MCP (HTTP)
                      │
                      ├── TOFU pinning: detect tool description changes
                      ├── ANSI sanitization: strip escape sequences from responses
                      ├── Guardrails (optional): filter prompt injection in responses
                      └── Quarantine: flag suspicious content for review
```

## Value for Our Setup

1. **CI log injection defense** — failed CI job logs could contain crafted text
   that manipulates the agent (e.g., fake error messages suggesting `git push --force`).
   The guardrail would detect and filter these.

2. **TOFU pinning** — alerts if the GitHub MCP server changes its tool definitions
   between sessions. Detects supply chain changes to the MCP server.

3. **ANSI sanitization** — strips hidden terminal escape sequences from GitHub API
   responses that could hide instructions from the user but be visible to the model.

4. **Audit trail** — quarantined responses are stored and reviewable via
   `--review-quarantine`.

## Integration Approach

### Current (direct HTTP)
```bash
claude mcp add-json github \
  '{"type":"http","url":"https://api.githubcopilot.com/mcp","headers":{...}}'
```

### With Protector (stdio wrapping HTTP)
```bash
claude mcp add-json github \
  '{"command":"mcp-context-protector","args":["--url","https://api.githubcopilot.com/mcp","--headers","Authorization: Bearer TOKEN","--headers","X-MCP-Toolsets: repos,pull_requests,actions"]}'
```

## Open Questions

### 1. Header forwarding
Does `--url` mode support custom headers (`Authorization`, `X-MCP-Toolsets`)?
The docs show `--url` but no header examples. Need to check source code or test.

### 2. LlamaFirewall dependency
Full guardrail protection requires LlamaFirewall (`--guardrail-provider LlamaFirewall`).
This is a separate ML model — likely large, adds significant image size and startup time.
Without it, only TOFU pinning and ANSI sanitization are available.
- Is TOFU + ANSI enough for our threat model?
- Can we run LlamaFirewall as a sidecar instead of in the same container?

### 3. Python / uv dependency
The protector uses Python + uv. We have Python 3 in the builder image but not uv.
Options:
- `pip install mcp-context-protector` (if published to PyPI)
- `uv sync` from git clone in Dockerfile
- Vendor the protector as a script in .devcontainer/

### 4. State persistence
TOFU pinned configs must persist across container restarts.
Options:
- Store in the `collector-dev-claude` volume (`/home/dev/.claude/`)
- Separate volume for protector state
- Environment variable for state directory

### 5. Startup latency
Adds another process to the MCP chain. Need to measure:
- Protector initialization time
- Per-request overhead (especially for large CI log responses)
- Impact on `/watch-ci` polling loop

### 6. Toolset filtering compatibility
We use `X-MCP-Toolsets: repos,pull_requests,actions` to limit which tools
the GitHub MCP server exposes. Need to verify the protector passes this
header through and doesn't interfere with tool discovery.

### 7. Quarantine review in headless mode
`--review-quarantine` is interactive. In autonomous mode (stream-json),
how would quarantined responses be surfaced? Options:
- Log to stderr (visible in stream output)
- Write to a file in /workspace for post-session review
- Fail the MCP call so the agent reports it as a blocker

## Threat Model Comparison

| Threat | Without Protector | With Protector |
|--------|------------------|----------------|
| CI log prompt injection | Unmitigated | Guardrail filters (with LlamaFirewall) |
| GitHub MCP tool definition change | Undetected | TOFU alerts on change |
| ANSI hidden instructions in responses | Unmitigated | Stripped and replaced |
| Token exfiltration via MCP responses | Unmitigated | Guardrail may detect |
| Credential in PR body/comments | Unmitigated | Guardrail may detect |
| MCP server impersonation | Undetected | TOFU pinning prevents |

## Recommendation

**Phase 1 (low effort):** Add TOFU pinning + ANSI sanitization only.
No LlamaFirewall. Validates tool definitions haven't changed, strips
escape sequences. Minimal dependencies, minimal latency.

**Phase 2 (medium effort):** Add LlamaFirewall as a sidecar container.
Full prompt injection defense for CI logs and GitHub API responses.
Requires docker-compose or multi-container setup.

**Phase 3 (high effort):** Custom guardrail rules specific to collector
development — detect patterns like "ignore previous instructions",
credential patterns in PR bodies, suspicious git commands in CI logs.

## Prerequisites Before Starting

- [ ] Verify `--url` supports custom headers (check source)
- [ ] Test basic TOFU mode without LlamaFirewall
- [ ] Measure startup latency overhead
- [ ] Confirm TOFU state directory is configurable
- [ ] Test with `X-MCP-Toolsets` header passthrough
