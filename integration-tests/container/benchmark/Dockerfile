# Based on https://github.com/phoronix-test-suite/phoronix-test-suite/blob/1fc8fbe76e256a6d438b05f6b8f04edd9f35c5e4/deploy/docker/ubuntu-pts-docker-build.sh

FROM ubuntu:22.04 as base

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y php-cli php-xml php-zip git
RUN git clone https://github.com/phoronix-test-suite/phoronix-test-suite.git
WORKDIR /phoronix-test-suite
RUN git checkout v10.8.4 && \
    rm -rf .git/ && \
    mkdir -p /var/lib/phoronix-test-suite/ && \
    ./phoronix-test-suite make-openbenchmarking-cache lean
RUN rm -f /var/lib/phoronix-test-suite/core.pt2so

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && \
    apt install -y \
        unzip \
        php-cli \
        apt-utils \
        mesa-utils \
        php-xml \
        git-core \
        apt-file \
        sudo \
        icecc && \
    apt-file update

COPY --from=base /var/lib/phoronix-test-suite /var/lib/phoronix-test-suite
COPY --from=base /phoronix-test-suite /phoronix-test-suite
COPY phoronix-test-suite.xml /etc/phoronix-test-suite.xml
COPY suite-definition.xml /var/lib/phoronix-test-suite/test-suites/local/collector/

ENTRYPOINT ["/phoronix-test-suite/phoronix-test-suite"]
