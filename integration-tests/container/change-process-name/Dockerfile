FROM ubuntu:jammy

COPY change-process-name.c /change-process-name.c

RUN apt update -y && apt install gcc lsof -y
RUN gcc change-process-name.c -o change-process-name

ENTRYPOINT /change-process-name
