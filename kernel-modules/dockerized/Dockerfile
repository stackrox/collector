FROM registry.access.redhat.com/ubi8/ubi:8.4 AS rhel-like

ARG USER
ARG PASS

RUN subscription-manager register --username $USER --password $PASS && \
	subscription-manager attach && \
	# this pool id may be associated with the current test user account.
	# if the account changes, use: `subscription-manager list --available` to find a valid id.
	subscription-manager attach --pool=8a85f9a178d12e760178dba5bb1d2b43 && \
	dnf config-manager --enable rhel-8-for-x86_64-baseos-rpms && \
	dnf config-manager --enable rhel-8-for-x86_64-appstream-rpms && \
	dnf -y update && \
	dnf -y install \
		make \
		cmake \
		gcc-c++ \
		llvm-7.0.1 \
		clang-7.0.1 \
		patch \
		elfutils-libelf \
		git

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts

FROM rhel-like AS patcher

ARG BRANCH=master
ARG LEGACY_PROBES=false

COPY /collector /collector

RUN /scripts/patch-files.sh $BRANCH $LEGACY_PROBES

FROM rhel-like AS builder

COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY /bundles /bundles

RUN /scripts/compile.sh

# Create a clean image leaving only the compiled kernel modules
FROM registry.access.redhat.com/ubi8/ubi:8.4

COPY --from=builder /kernel-modules /kernel-modules
COPY --from=patcher /kobuild-tmp/latest-version /latest-version

ENTRYPOINT [ "/bin/bash", "-c" ]
