FROM python:2.7-slim

RUN apt-get update && \
    apt-get install --no-install-recommends -y python3=3.4.2-2 && \
    rm -rf /var/lib/apt

RUN pip install --no-cache-dir lxml==4.2.4

COPY ./kernel-crawler.py \
     ./apt-crawler.py \
     ./kope.io.asc \
     /

ENTRYPOINT ["python", "kernel-crawler.py"]
