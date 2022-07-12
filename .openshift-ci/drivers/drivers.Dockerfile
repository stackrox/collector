FROM pipeline:fc36-base

COPY --from=pre-build:latest /kernel-modules /kernel-modules
COPY --from=fc36-drivers:latest /kernel-modules /kernel-modules
COPY --from=fc36-drivers:latest /FAILURES /FAILURES
COPY --from=rhel8-drivers:latest /kernel-modules /kernel-modules
COPY --from=rhel8-drivers:latest /FAILURES /FAILURES
COPY --from=rhel7-drivers:latest /kernel-modules /kernel-modules
COPY --from=rhel7-drivers:latest /FAILURES /FAILURES

COPY /.circleci/kernel-module-build-failures-check/20-test-for-build-failures.sh /scripts/drivers-build-failures.sh
COPY /.circleci/gcloud-init/setup-gcp-env.sh /scripts/setup-gcp-env.sh

COPY /.openshift-ci/drivers/scripts/push-drivers.sh /scripts/push-drivers.sh
