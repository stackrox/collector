FROM --platform=linux/amd64 pstauffer/curl:latest as stage-amd64
FROM --platform=linux/ppc64le quay.io/rhacs-eng/pstauffer-curl-ppc64le:latest as stage-ppc64le

# Declare TARGETARCH to make it available
ARG TARGETARCH

# Select final stage based on TARGETARCH ARG
FROM stage-${TARGETARCH} as final

COPY schedule-curls.sh /usr/bin/schedule-curls.sh 
