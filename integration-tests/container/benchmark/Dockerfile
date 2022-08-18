# Declare TARGETARCH to make it available
ARG TARGETARCH

FROM --platform=linux/amd64 phoronix/pts as stage-x86_64
FROM --platform=linux/ppc64le quay.io/rhacs-eng/phoronix-pts-ppc64le as stage-ppc64le

# Select final stage based on TARGETARCH ARG
FROM stage-${TARGETARCH} as final

# Commands run at final stage for all target platforms
ADD phoronix-test-suite.xml /etc/phoronix-test-suite.xml
ADD suite-definition.xml/ /var/lib/phoronix-test-suite/test-suites/local/collector/

ENV DEBIAN_FRONTEND=noninteractive

# Needed by the hackbench benchmark
RUN apt-get update && \
    apt-get install -y icecc

ENTRYPOINT ["/phoronix-test-suite/phoronix-test-suite"]
