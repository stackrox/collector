FROM python:2.7-slim

RUN apt-get update && \
    apt-get install --no-install-recommends -y python3>=3.5 python3-lxml && \
    rm -rf /var/lib/apt

RUN pip install --no-cache-dir lxml==4.2.4

COPY ./kernel-crawler.py \
     ./repo-crawler.py \
     ./kope.io.asc \
     /

ENTRYPOINT ["python3", "kernel-crawler.py"]
