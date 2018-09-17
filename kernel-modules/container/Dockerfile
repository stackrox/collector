FROM alpine:3.8

RUN mkdir /kernel-modules/

COPY ./kernel-modules /kernel-modules/

RUN find /kernel-modules -name '*.ko' -exec gzip {} \;

FROM scratch

COPY --from=0 /kernel-modules/ /kernel-modules/
