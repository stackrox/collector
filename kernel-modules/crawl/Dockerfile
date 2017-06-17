FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      python \
      python-lxml \
      python-requests \
    ;

COPY ./kernel-crawler.py /

ENTRYPOINT ["/kernel-crawler.py"]
