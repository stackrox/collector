FROM ubuntu:jammy

COPY so_reuseport.py so_reuseport.py

RUN apt update -y && apt install python3 -y

ENTRYPOINT python3 so_reuseport.py & python3 so_reuseport.py meaningless_argument
