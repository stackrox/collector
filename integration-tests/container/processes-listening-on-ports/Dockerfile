FROM ubuntu:jammy

COPY process-listening-on-ports.c /process-listening-on-ports.c
COPY change-process-name.c /change-process-name.c
COPY change-executable-file-path.c /change-executable-file-path.c
COPY listening-endpoint-child-process.c /listening-endpoint-child-process.c 

RUN apt update -y && apt install gcc lsof net-tools -y
RUN gcc process-listening-on-ports.c -o process-listening-on-ports
RUN gcc change-process-name.c -o change-process-name
RUN gcc change-executable-file-path.c -o change-executable-file-path
RUN gcc listening-endpoint-child-process.c -o listening-endpoint-child-process

RUN ln -s /process-listening-on-ports plop

ENTRYPOINT /process-listening-on-ports
