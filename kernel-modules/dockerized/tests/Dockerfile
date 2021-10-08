FROM kernel-builder:latest AS drivers

FROM stackrox/collector:3.4.1-slim

COPY --from=drivers /kernel-modules/b6745d795b8497aaf387843dc8aa07463c944d3ad67288389b754daaebea4b62/*.gz /kernel-modules/

RUN ls -la /kernel-modules
