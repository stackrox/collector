ARG USER
ARG PASS

FROM registry.access.redhat.com/ubi8/ubi:8.5 AS rhel-8-base

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
		elfutils-libelf-devel \
		git

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts

FROM registry.access.redhat.com/ubi7/ubi:7.9 AS rhel-7-base

ARG USER
ARG PASS

RUN subscription-manager register --username $USER --password $PASS && \
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
		elfutils-libelf

# This directory goes separately to prevent it from being modified/deleted when switching branches
COPY /collector/kernel-modules/dockerized/scripts /scripts

FROM rhel-8-base AS patcher

ARG BRANCH=master
ARG LEGACY_PROBES=false

COPY /collector /collector

RUN /scripts/patch-files.sh $BRANCH $LEGACY_PROBES

FROM rhel-8-base AS rhel-8-builder

COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY /bundles /bundles

# This stage will only build bundles for kernel 4+
RUN find /bundles/ -type f -name "bundle-[0-3].*" -delete

RUN /scripts/compile.sh

FROM rhel-7-base AS rhel-7-builder

COPY --from=patcher /kobuild-tmp/versions-src /kobuild-tmp/versions-src
COPY /bundles /bundles

# This stage will only build bundles for kernel 0-3
RUN find /bundles/ -type f -name "bundle-[4-9].*" -delete

RUN scl enable llvm-toolset-7.0 /scripts/compile.sh

# Create a clean image leaving only the compiled kernel modules
FROM registry.access.redhat.com/ubi8/ubi:8.5

COPY --from=rhel-8-builder /kernel-modules /kernel-modules
COPY --from=rhel-7-builder /kernel-modules /kernel-modules

ENTRYPOINT [ "/bin/bash", "-c" ]
