ARG REDHAT_USERNAME
ARG REDHAT_PASSWORD

ARG CACHE_REPO
ARG CACHE_TAG

ARG CHECKOUT_BEFORE_PATCHING=true

FROM registry.access.redhat.com/ubi8/ubi:8.5 AS rhel-8-base

ARG REDHAT_USERNAME
ARG REDHAT_PASSWORD

ENV DOCKERIZED=1

RUN subscription-manager register --username $REDHAT_USERNAME --password $REDHAT_PASSWORD && \
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
		elfutils-libelf-devel \
		git \
		python3 \
		kmod && \
	ln -s /usr/bin/gcc /usr/bin/gcc-8

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts
COPY /collector/kernel-modules/build/prepare-src /scripts/prepare-src.sh
COPY /collector/kernel-modules/build/build-kos /usr/bin
COPY /collector/kernel-modules/build/build-wrapper.sh /scripts/compile.sh

FROM rhel-8-base AS patcher

ARG BRANCH=master
ARG LEGACY_PROBES=false

COPY /collector /collector

ENV CHECKOUT_BEFORE_PATCHING=$CHECKOUT_BEFORE_PATCHING
RUN /scripts/patch-files.sh $BRANCH $LEGACY_PROBES

FROM $CACHE_REPO/collector-drivers:$CACHE_TAG AS cache

FROM rhel-8-base AS task-master

ARG USE_KERNELS_FILE=false
ENV USE_KERNELS_FILE=$USE_KERNELS_FILE

COPY /bundles /bundles
COPY /collector/kernel-modules/build/apply-blocklist.py /scripts
COPY /collector/kernel-modules/BLOCKLIST /scripts
COPY /collector/kernel-modules/dockerized/BLOCKLIST /scripts/dockerized/
COPY /collector/kernel-modules/KERNEL_VERSIONS /KERNEL_VERSIONS
COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY --from=cache /kernel-modules/ /kernel-modules/

RUN /scripts/get-build-tasks.sh; rm -rf /bundles/ /kobuild-tmp/ /kernel-modules/

FROM rhel-8-base AS rhel-8-builder

COPY /bundles /bundles
COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY --from=task-master /build-tasks /build-tasks

# This stage will only build bundles for kernel 4+
RUN sed -i '/^[0-3]\./d' /build-tasks; \
	/scripts/compile.sh </build-tasks; \
	rm -rf /bundles/

FROM registry.access.redhat.com/ubi7/ubi:7.9 AS rhel-7-base

ARG REDHAT_USERNAME
ARG REDHAT_PASSWORD

ENV DOCKERIZED=1

RUN subscription-manager register --username $REDHAT_USERNAME --password $REDHAT_PASSWORD && \
	subscription-manager attach && \
	# this pool id may be associated with the current test user account.
	# if the account changes, use: `subscription-manager list --available` to find a valid id.
	subscription-manager attach --pool=8a85f9a178d12e760178dba5bb1d2b43 && \
	subscription-manager repos --enable rhel-7-server-devtools-rpms && \
	subscription-manager repos --enable rhel-server-rhscl-7-rpms && \
	yum -y update && \
	yum -y install \
		make \
		gcc-c++ \
		llvm-toolset-7.0 \
		elfutils-libelf \
		kmod

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts
COPY /collector/kernel-modules/build/prepare-src /scripts/prepare-src.sh
COPY /collector/kernel-modules/build/build-kos /usr/bin
COPY /collector/kernel-modules/build/build-wrapper.sh /scripts/compile.sh

FROM rhel-7-base AS rhel-7-builder

COPY /bundles /bundles
COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY --from=task-master /build-tasks /build-tasks

# This stage will only build bundles for kernel 0-3
RUN sed -i '/^[4-9]\./d' /build-tasks; \
	scl enable llvm-toolset-7.0 /scripts/compile.sh </build-tasks; \
	rm -rf /bundles/

# Create a clean image leaving only the compiled kernel modules
FROM registry.access.redhat.com/ubi8/ubi:8.5

COPY --from=rhel-8-builder /kernel-modules /kernel-modules
COPY --from=rhel-7-builder /kernel-modules /kernel-modules
COPY --from=rhel-8-builder /FAILURES /FAILURES
COPY --from=rhel-7-builder /FAILURES /FAILURES

ENTRYPOINT [ "/bin/bash", "-c" ]
