FROM ubuntu:jammy

COPY process-listening-on-ports.c /process-listening-on-ports.c
COPY change-process-name.c /change-process-name.c

RUN apt update -y && apt install gcc lsof net-tools -y
RUN gcc process-listening-on-ports.c -o process-listening-on-ports
RUN gcc change-process-name.c -o change-process-name

RUN ln -s /process-listening-on-ports plop

ENTRYPOINT /process-listening-on-ports
