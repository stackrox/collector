FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      python \
      python-lxml \
      python-requests \
    ;

COPY ./kernel-crawler.py /
COPY ./crawl-and-sort.sh /

ENTRYPOINT ["/crawl-and-sort.sh"]
