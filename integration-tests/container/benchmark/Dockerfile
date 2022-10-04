FROM phoronix/pts

ADD phoronix-test-suite.xml /etc/phoronix-test-suite.xml
ADD suite-definition.xml/ /var/lib/phoronix-test-suite/test-suites/local/collector/

ENV DEBIAN_FRONTEND=noninteractive

# Needed by the hackbench benchmark
RUN apt-get update && \
    apt-get install -y icecc

ENTRYPOINT ["/phoronix-test-suite/phoronix-test-suite"]
