FROM ubuntu:jammy

COPY sched_process_exit.c /sched_process_exit.c
COPY entrypoint.sh /entrypoint.sh

RUN apt update -y && apt install gcc -y
RUN gcc sched_process_exit.c -o sched_process_exit

ENTRYPOINT /entrypoint.sh
