FROM ubuntu:jammy

COPY listening-endpoint-child-process.c /listening-endpoint-child-process.c

RUN apt update -y && apt install gcc lsof -y
RUN gcc listening-endpoint-child-process.c -o listening-endpoint-child-process

ENTRYPOINT /listening-endpoint-child-process
