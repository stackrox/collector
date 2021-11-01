ARG COLLECTOR_TAG

FROM kernel-builder:latest AS drivers

FROM stackrox/collector:$COLLECTOR_TAG

COPY --from=drivers /kernel-modules/ /kernel-modules/
COPY --from=drivers /latest-version /latest-version
# Make modules for the latest version (see patch-files.sh) available in /kernel-modules
RUN mv -v /kernel-modules/$(cat /latest-version)/*.gz /kernel-modules/
# Remove all versioned subdirectories under /kernel-modules
RUN find /kernel-modules/ -mindepth 1 -type d -delete

RUN ls -la /kernel-modules
