FROM ubuntu:jammy

COPY process-listening-on-ports.c /process-listening-on-ports.c

RUN apt update -y && apt install gcc lsof -y
RUN gcc process-listening-on-ports.c -o process-listening-on-ports

EXPOSE 8081 8081
EXPOSE 9091 9091

ENTRYPOINT /process-listening-on-ports
