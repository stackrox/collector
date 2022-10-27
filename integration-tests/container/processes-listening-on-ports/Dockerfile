FROM ubuntu:jammy

COPY process-listening-on-ports.c /process-listening-on-ports.c

RUN apt update -y && apt install gcc lsof -y
RUN gcc process-listening-on-ports.c -o process-listening-on-ports

ENTRYPOINT /process-listening-on-ports
