FROM replaced-by-osci:bin AS pr-data

RUN .openshift-ci/jobs/apollo-src/dump-pr-data.sh

FROM quay.io/stackrox-io/apollo-ci:collector-0.3.56

COPY --from=pr-data /ci-data/ /ci-data/
COPY --from=pr-data /go/src/github.com/stackrox/collector/ /go/src/github.com/stackrox/collector/

# TODO: move this to the build for collector's apollo-ci image
RUN set -ex \
 && wget --no-verbose -O oc.tgz https://github.com/openshift/origin/releases/download/v3.11.0/openshift-origin-client-tools-v3.11.0-0cbc58b-linux-64bit.tar.gz \
 && tar -xf oc.tgz \
 && install openshift-origin-client-tools-v3.11.0-0cbc58b-linux-64bit/oc /usr/local/bin \
 && rm -rf openshift-* oc.tgz \
 && command -v oc

WORKDIR /go/src/github.com/stackrox/collector/
