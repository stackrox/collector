FROM ubuntu:jammy

COPY thread_exec.c /thread_exec.c

RUN apt update -y && apt install gcc -y && \
    gcc -lpthread thread_exec.c -o thread_exec

ENTRYPOINT /thread_exec
