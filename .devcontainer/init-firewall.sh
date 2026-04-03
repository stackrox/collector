#!/usr/bin/env bash
# Optional network firewall for use with --dangerously-skip-permissions mode.
# Restricts outbound traffic to only necessary services.
# Requires: --cap-add=NET_ADMIN on the container (not set by default).
# To enable: add "--cap-add=NET_ADMIN" to runArgs in devcontainer.json.

set -euo pipefail

if ! command -v iptables &>/dev/null; then
    echo "iptables not available, skipping firewall setup"
    exit 0
fi

if ! iptables -L &>/dev/null 2>&1; then
    echo "No NET_ADMIN capability, skipping firewall setup"
    exit 0
fi

echo "Configuring network firewall..."

# Allow loopback
iptables -A OUTPUT -o lo -j ACCEPT

# Allow established connections
iptables -A OUTPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# Allow DNS
iptables -A OUTPUT -p udp --dport 53 -j ACCEPT
iptables -A OUTPUT -p tcp --dport 53 -j ACCEPT

# Allow GCP / Vertex AI (Claude Code backend + gcloud CLI + VM management)
# Vertex AI endpoints: https://{REGION}-aiplatform.googleapis.com
iptables -A OUTPUT -d oauth2.googleapis.com -j ACCEPT
iptables -A OUTPUT -d accounts.google.com -j ACCEPT
iptables -A OUTPUT -d www.googleapis.com -j ACCEPT
iptables -A OUTPUT -d storage.googleapis.com -j ACCEPT
iptables -A OUTPUT -d compute.googleapis.com -j ACCEPT
iptables -A OUTPUT -d cloudresourcemanager.googleapis.com -j ACCEPT
# Vertex AI regions (allow all *.googleapis.com via port 443)
iptables -A OUTPUT -p tcp --dport 443 -d us-central1-aiplatform.googleapis.com -j ACCEPT
iptables -A OUTPUT -p tcp --dport 443 -d us-east5-aiplatform.googleapis.com -j ACCEPT
iptables -A OUTPUT -p tcp --dport 443 -d europe-west1-aiplatform.googleapis.com -j ACCEPT
iptables -A OUTPUT -d metadata.google.internal -j ACCEPT

# Allow Claude API (direct Anthropic, if used alongside or instead of Vertex)
iptables -A OUTPUT -d api.anthropic.com -j ACCEPT
iptables -A OUTPUT -d statsig.anthropic.com -j ACCEPT
iptables -A OUTPUT -d sentry.io -j ACCEPT

# Allow GitHub (for git push, gh CLI, API)
iptables -A OUTPUT -d github.com -j ACCEPT
iptables -A OUTPUT -d api.github.com -j ACCEPT

# Allow container registries
iptables -A OUTPUT -d quay.io -j ACCEPT
iptables -A OUTPUT -d cdn.quay.io -j ACCEPT
iptables -A OUTPUT -d cdn01.quay.io -j ACCEPT
iptables -A OUTPUT -d cdn02.quay.io -j ACCEPT
iptables -A OUTPUT -d cdn03.quay.io -j ACCEPT
iptables -A OUTPUT -d registry.access.redhat.com -j ACCEPT

# Allow SSH (for GCP VM access during integration testing)
iptables -A OUTPUT -p tcp --dport 22 -j ACCEPT

# Allow npm registry
iptables -A OUTPUT -d registry.npmjs.org -j ACCEPT

# Allow Go module proxy
iptables -A OUTPUT -d proxy.golang.org -j ACCEPT
iptables -A OUTPUT -d sum.golang.org -j ACCEPT

# Drop everything else
iptables -A OUTPUT -j DROP

echo "Firewall configured."
