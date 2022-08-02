FROM --platform=linux/amd64 docker:dind as stage-amd64
FROM --platform=linux/ppc64le ppc64le/docker:dind as stage-ppc64le

# Declare TARGETARCH to make it available
ARG TARGETARCH

# Select final stage based on TARGETARCH ARG
FROM stage-${TARGETARCH} as final

# Commands run at final stage for all target platforms
ADD stats.sh /

ENTRYPOINT ["/stats.sh"]
