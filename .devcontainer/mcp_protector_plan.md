# MCP Security Proxy Plan

Security proxy options for protecting Claude Code ↔ GitHub MCP communication.

## Option 1: mcp-watchdog (Recommended)

[bountyyfi/mcp-watchdog](https://github.com/bountyyfi/mcp-watchdog) — lightweight,
pattern-based security proxy. No ML models required. 273+ tests.

### What it does

- **Credential redaction** — catches 30+ secret patterns (GitHub PATs, AWS keys, JWTs,
  etc.) in MCP responses. Prevents token leakage via PR bodies, commit messages, CI logs.
- **Prompt injection detection** — `<IMPORTANT>` tag injection, role injection markers,
  SANDWORM-style instructions, homoglyph evasion, HTML-encoded variants.
- **Tool integrity (rug pull detection)** — hashes tool definitions, alerts on schema changes.
- **ANSI/Unicode sanitization** — strips zero-width chars, bidirectional overrides,
  escape sequences that hide instructions from users.
- **Command/SQL/SSRF injection** — shell metacharacters, reverse shell patterns,
  cloud metadata access (AWS IMDS, GCP, Azure).
- **Filesystem scope enforcement** — blocks writes to `.git/config`, `.ssh/`, `.aws/`.
- **Rate limiting** — consent fatigue protection, notification injection blocking.
- **Cross-server flow tracking** — detects token propagation between MCP servers.

### Dependencies

- Python 3.10+ (already in our container)
- `pip install mcp-watchdog` — core is pattern matching + entropy, no ML
- Optional `[semantic]` extra — adds Claude Haiku classifier (opt-in, not needed)
- Optional `[filesystem]` extra — adds inotify/FSEvents monitoring

### Integration

```bash
# In entrypoint.sh, wrap the GitHub MCP server:
claude mcp add-json github \
  '{"command":"mcp-watchdog","args":["--verbose","--url","https://api.githubcopilot.com/mcp","--headers","Authorization: Bearer TOKEN","--headers","X-MCP-Toolsets: repos,pull_requests,actions"]}'
```

### Open questions

- [ ] Does `--url` mode support custom headers for HTTP upstream?
- [ ] Does `X-MCP-Toolsets` pass through the proxy correctly?
- [ ] Where does watchdog store tool integrity hashes? (needs persistence in volume)
- [ ] What's the per-request latency overhead for large CI log responses?
- [ ] How does `--verbose` output surface in headless/stream-json mode?

### Install in Dockerfile

```dockerfile
RUN pip3 install mcp-watchdog
```

Estimated size: ~10-20MB (pattern matching only, no ML).

---

## Option 2: mcp-context-protector (Heavy)

[trailofbits/mcp-context-protector](https://github.com/trailofbits/mcp-context-protector) —
Trail of Bits security wrapper with ML-based guardrails.

### What it does

- **TOFU pinning** — records tool definitions on first use, alerts on changes.
- **ANSI sanitization** — strips escape sequences.
- **LlamaFirewall guardrails** — ML-based prompt injection detection in responses.
- **Quarantine** — flags suspicious responses for manual review.

### Why not (for now)

- **LlamaFirewall is a hard dependency** — `llamafirewall>=1.0.3` in pyproject.toml,
  not optional. Pulls in PyTorch, transformers, huggingface_hub, semgrep.
- **Adds ~3GB** to the container image.
- **Heavy per-request cost** — ML inference on every MCP response.
- **Better suited for centralized deployment** (shared proxy for team) rather
  than per-devcontainer.

### When to reconsider

- If Trail of Bits makes LlamaFirewall optional (`pip install mcp-context-protector[guardrails]`)
- If deploying a shared MCP proxy for the whole team (not per-container)
- If prompt injection via CI logs becomes a demonstrated threat (not just theoretical)

---

## Option 3: open-mcp-guardrails

[interactive-inc/open-mcp-guardrails](https://github.com/interactive-inc/open-mcp-guardrails) —
policy-based guardrails proxy. Early stage (0 stars).

- PII leak detection
- Secret exposure prevention
- Prompt injection blocking
- Policy-based access control

Too early to evaluate. Worth watching.

---

## Threat Model

| Threat | No proxy | mcp-watchdog | mcp-context-protector |
|--------|----------|-------------|----------------------|
| Credential leak in MCP response | Unmitigated | 30+ patterns redacted | Guardrail may detect |
| Prompt injection in CI logs | Unmitigated | 70+ patterns blocked | ML-based detection |
| Tool definition change (rug pull) | Undetected | Hash-based detection | TOFU pinning |
| ANSI/Unicode hidden instructions | Unmitigated | Stripped | Stripped |
| SSRF to cloud metadata | Unmitigated | Blocked | Not covered |
| Command injection via MCP | Unmitigated | Shell/SQL patterns blocked | Not covered |
| Cross-server token propagation | Unmitigated | Flow tracking | Not covered |

## Recommendation

**Start with mcp-watchdog.** It covers more attack vectors than mcp-context-protector,
has no ML dependencies, and is designed for exactly our use case (wrapping MCP servers
for AI coding assistants). The credential redaction alone justifies the ~10MB install.

### Implementation steps

1. Test `mcp-watchdog --url` with custom headers locally
2. Add `pip3 install mcp-watchdog` to Dockerfile
3. Update entrypoint.sh to wrap GitHub MCP with watchdog
4. Verify `X-MCP-Toolsets` header passes through
5. Configure watchdog state persistence in `collector-dev-claude` volume
6. Test with `/watch-ci` to ensure CI log scanning works
