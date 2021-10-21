ARG COLLECTOR_TAG

FROM kernel-builder:latest AS drivers

FROM stackrox/collector:$COLLECTOR_TAG

ARG DRIVER_VERSION=b6745d795b8497aaf387843dc8aa07463c944d3ad67288389b754daaebea4b62

COPY --from=drivers /kernel-modules/${DRIVER_VERSION}/*.gz /kernel-modules/

RUN ls -la /kernel-modules
