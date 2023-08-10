FROM quay.io/centos/centos:stream9 AS target

RUN dnf install -y --nobest libcurl

FROM target AS build

RUN dnf install -y --nobest gcc libcurl-devel

WORKDIR /

COPY curl-test.c .

RUN cc $(pkg-config --cflags libcurl) -o curl-test curl-test.c $(pkg-config --libs libcurl)

FROM target

COPY --from=build /curl-test /

ENTRYPOINT ["/curl-test"]
