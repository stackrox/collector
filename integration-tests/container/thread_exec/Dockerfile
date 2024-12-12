FROM ubuntu:jammy

COPY thread_exec.c /thread_exec.c

RUN apt update -y && apt install gcc -y && \
    gcc -lpthread thread_exec.c -o /usr/bin/thread_exec

# XXX: s390x reports task_comm with a leading slash if the entrypoint
# will have it:
#
# [TRACE] (Service.cpp:156) /thread_exec (52684) < execve res=0
# exe=/thread_exec args=NULL tid=52684(/thread_exec) pid=52684(/thread_exec)
# ptid=52664(sh) cwd=<NA> comm=/thread_exec trusted_exepath=/thread_exec ...
#
# It looks like we don't exercise anything similar in other tests, so just make
# sure the binary is in PATH for now.

ENTRYPOINT thread_exec
