# Collector development container
# Based on the collector-builder image which has all C++ dependencies pre-installed.
# Adds Claude Code, Go, and developer tooling for agent-driven development.
#
# Build environment: CentOS Stream 10 with clang, llvm, cmake, grpc, protobuf,
# libbpf, bpftool, and all other collector dependencies.

ARG COLLECTOR_BUILDER_TAG=master
FROM quay.io/stackrox-io/collector-builder:${COLLECTOR_BUILDER_TAG}

# Install developer tooling not in the builder image
# Note: git, findutils, which, openssh-clients already in builder
# bubblewrap: Claude Code uses this for built-in command sandboxing
RUN dnf install -y \
        bubblewrap \
        jq \
        socat \
        zsh \
        procps-ng \
        sudo \
        python3-pip \
        iptables \
        ipset \
    && dnf clean all

# Determine architecture strings used by various download URLs
# uname -m gives aarch64 or x86_64
# Go uses arm64/amd64, ripgrep/fd use aarch64/x86_64
RUN ARCH=$(uname -m) \
    && GOARCH=$([ "$ARCH" = "aarch64" ] && echo "arm64" || echo "amd64") \
    # Install Go
    && curl -fsSL "https://go.dev/dl/go1.23.6.linux-${GOARCH}.tar.gz" | tar -C /usr/local -xzf - \
    # Install ripgrep
    && curl -fsSL "https://github.com/BurntSushi/ripgrep/releases/download/14.1.1/ripgrep-14.1.1-${ARCH}-unknown-linux-gnu.tar.gz" \
       | tar -xzf - --strip-components=1 -C /usr/local/bin "ripgrep-14.1.1-${ARCH}-unknown-linux-gnu/rg" \
    # Install fd
    && curl -fsSL "https://github.com/sharkdp/fd/releases/download/v10.2.0/fd-v10.2.0-${ARCH}-unknown-linux-gnu.tar.gz" \
       | tar -xzf - --strip-components=1 -C /usr/local/bin "fd-v10.2.0-${ARCH}-unknown-linux-gnu/fd"

ENV PATH="/usr/local/go/bin:${PATH}"
ENV GOPATH="/home/dev/go"
ENV PATH="${GOPATH}/bin:${PATH}"

# Install Node.js (needed for Claude Code)
ARG NODE_VERSION=22
RUN curl -fsSL https://rpm.nodesource.com/setup_${NODE_VERSION}.x | bash - \
    && dnf install -y nodejs \
    && dnf clean all

# Install Claude Code
RUN npm install -g @anthropic-ai/claude-code

# Install gcloud CLI (for Vertex AI auth and GCP VM management)
RUN curl -fsSL https://sdk.cloud.google.com > /tmp/install-gcloud.sh \
    && bash /tmp/install-gcloud.sh --disable-prompts --install-dir=/opt \
    && rm /tmp/install-gcloud.sh
ENV PATH="/opt/google-cloud-sdk/bin:${PATH}"

# Pull GitHub MCP server image (used by Claude Code for GitHub operations)
# Configured in .claude/settings.json as an MCP server

# Create non-root dev user with passwordless sudo
RUN useradd -m -s /bin/zsh dev \
    && echo "dev ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/dev

# Install ansible for VM-based testing (optional, lightweight)
RUN pip3 install ansible-core

# Firewall script for network isolation (optional, used with --dangerously-skip-permissions)
COPY init-firewall.sh /usr/local/bin/init-firewall.sh
RUN chmod +x /usr/local/bin/init-firewall.sh

USER dev
WORKDIR /workspace

# Persist shell history and Claude state across rebuilds (volumes in devcontainer.json)
ENV HISTFILE=/home/dev/.commandhistory/.zsh_history

ENV SHELL=/bin/zsh
ENV DEVCONTAINER=true
