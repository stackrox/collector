FROM ubuntu:jammy

COPY defunct.c /defunct.c

RUN apt update -y && apt install gcc -y
RUN gcc defunct.c -o defunct

ENTRYPOINT /bin/bash -c "for ((i=0;i<1000;i=i+1)); do sleep 0.1; /defunct & done; sleep 600" 
