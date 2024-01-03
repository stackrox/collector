ARG TEST_RHEL_PACKAGE=snappy

# Must stay empty so we find 100% original container aliased as ubi-normal.
FROM registry.access.redhat.com/ubi8/ubi-minimal:latest AS ubi-minimal

# The installer must be ubi (not minimal) and must be 8.9 or later since the earlier versions complain:
#  subscription-manager is disabled when running inside a container. Please refer to your host system for subscription management.
FROM registry.access.redhat.com/ubi8/ubi:latest AS installer

FROM installer AS test-yes-entitlement-minimal

COPY --from=ubi-minimal / /mnt
COPY ./.rhtap /tmp/.rhtap

ARG TEST_RHEL_PACKAGE
RUN /tmp/.rhtap/scripts/subscription-manager-bro.sh register && \
    dnf -y --installroot=/mnt install "$TEST_RHEL_PACKAGE" && \
    dnf -y --installroot=/mnt remove "$TEST_RHEL_PACKAGE" && \
    /tmp/.rhtap/scripts/subscription-manager-bro.sh cleanup
