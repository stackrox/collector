FROM ubuntu:jammy

RUN apt update -y && apt install lsof python3 socat -y

COPY open-close-ports-load.py /open-close-ports-load.py

ENTRYPOINT sleep 1000000
