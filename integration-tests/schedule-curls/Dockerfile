FROM debian:buster

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install -y \
	curl;

RUN apt-get autoremove -y

COPY schedule-curls.sh /usr/bin/schedule-curls.sh 
