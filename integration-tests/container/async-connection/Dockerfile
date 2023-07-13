FROM ubuntu:jammy AS target

RUN apt update -y && apt install -y libcurl4

FROM target AS build

RUN apt install -y build-essential pkg-config libcurl4-openssl-dev

WORKDIR /

COPY curl-test.c .

RUN cc $(pkg-config --cflags libcurl) -o curl-test curl-test.c $(pkg-config --libs libcurl)

FROM target

COPY --from=build /curl-test /

ENTRYPOINT ["/curl-test"]
