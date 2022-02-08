FROM debian:buster

RUN apt-get update \
 && apt-get upgrade -y \
 && apt-get install --no-install-recommends -y \
	curl;


COPY entrypoint /usr/bin/entrypoint 

RUN ls -l /usr/bin/entrypoint 

ENTRYPOINT ["/usr/bin/entrypoint"]
