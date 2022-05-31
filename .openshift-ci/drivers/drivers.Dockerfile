FROM registry.fedoraproject.org/fedora:36

COPY --from=pre-build:latest /kernel-modules /kernel-modules
COPY --from=fc36-drivers:latest /kernel-modules /kernel-modules
COPY --from=fc36-drivers:latest /FAILURES /FAILURES
COPY --from=rhel8-drivers:latest /kernel-modules /kernel-modules
COPY --from=rhel8-drivers:latest /FAILURES /FAILURES
COPY --from=rhel7-drivers:latest /kernel-modules /kernel-modules
COPY --from=rhel7-drivers:latest /FAILURES /FAILURES
COPY /.circleci/kernel-module-build-failures-check/20-test-for-build-failures.sh /scripts/drivers-build-failures.sh
