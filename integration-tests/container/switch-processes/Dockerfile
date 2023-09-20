FROM ubuntu:jammy

COPY switch-processes.c /switch-processes.c
COPY second-process.c /second-process.c

RUN apt update -y && apt install gcc lsof -y
RUN gcc switch-processes.c -o switch-processes
RUN gcc second-process.c -o second-process

ENTRYPOINT /switch-processes; sleep 100000
