FROM python:3.12

RUN mkdir /app
WORKDIR /app

COPY plop.py .
COPY requirements.txt .

RUN python3 -m pip install --no-cache-dir -r requirements.txt && \
    ln -s /usr/local/bin/flask plop

EXPOSE 5000

ENTRYPOINT ["flask", "--app", "plop.py", "run", "-h", "0.0.0.0"]
