ARG REDHAT_SUBSCRIPTION_ORG_ID
ARG REDHAT_SUBSCRIPTION_ACTIVATION_KEY

ARG CACHE_REPO
ARG CACHE_TAG

ARG CHECKOUT_BEFORE_PATCHING=true

FROM registry.access.redhat.com/ubi8/ubi:8.6 AS rhel-8-base

ARG REDHAT_SUBSCRIPTION_ORG_ID
ARG REDHAT_SUBSCRIPTION_ACTIVATION_KEY

ENV DOCKERIZED=1

# Subscription registration is only needed to install RHEL packages
# if on a non-RHEL host. (See https://access.redhat.com/solutions/5558771)
RUN subscription-manager register \
	--org "${REDHAT_SUBSCRIPTION_ORG_ID}" \
	--activationkey "${REDHAT_SUBSCRIPTION_ACTIVATION_KEY}" && \
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
	ln -s /usr/bin/gcc /usr/bin/gcc-8 && \
	subscription-manager unregister

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts
COPY /collector/kernel-modules/build/prepare-src /scripts/prepare-src.sh
COPY /collector/kernel-modules/build/build-kos /scripts/
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

ARG REDHAT_SUBSCRIPTION_ORG_ID
ARG REDHAT_SUBSCRIPTION_ACTIVATION_KEY

ENV DOCKERIZED=1

# Subscription registration is only needed to install RHEL packages
# if on a non-RHEL host. (See https://access.redhat.com/solutions/5558771)
RUN subscription-manager register \
	--org "${REDHAT_SUBSCRIPTION_ORG_ID}" \
	--activationkey "${REDHAT_SUBSCRIPTION_ACTIVATION_KEY}" && \
	subscription-manager repos --enable rhel-7-server-devtools-rpms && \
	subscription-manager repos --enable rhel-server-rhscl-7-rpms && \
	yum -y update && \
	yum -y install \
		make \
		gcc-c++ \
		llvm-toolset-7.0 \
		elfutils-libelf \
		kmod && \
	subscription-manager unregister

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts
COPY /collector/kernel-modules/build/prepare-src /scripts/prepare-src.sh
COPY /collector/kernel-modules/build/build-kos /scripts/
COPY /collector/kernel-modules/build/build-wrapper.sh /scripts/compile.sh
COPY /collector/kernel-modules/probe/ /probe

FROM rhel-7-base AS rhel-7-builder

COPY /bundles /bundles
COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY --from=task-master /build-tasks /build-tasks

# This stage will only build bundles for kernel 0-3
RUN sed -i '/^[4-9]\./d' /build-tasks; \
	scl enable llvm-toolset-7.0 /scripts/compile.sh </build-tasks; \
	rm -rf /bundles/

# Create a clean image leaving only the compiled kernel modules
FROM registry.access.redhat.com/ubi8/ubi:8.6

COPY --from=rhel-8-builder /kernel-modules /kernel-modules
COPY --from=rhel-7-builder /kernel-modules /kernel-modules
COPY --from=rhel-8-builder /FAILURES /FAILURES
COPY --from=rhel-7-builder /FAILURES /FAILURES

ENTRYPOINT [ "/bin/bash", "-c" ]
