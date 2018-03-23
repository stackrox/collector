FROM python:2.7-slim

RUN pip install --no-cache-dir lxml

COPY ./kernel-crawler.py /

ENTRYPOINT ["python", "kernel-crawler.py"]
