FROM debian:stretch-slim

RUN apt update -y && apt install -y debian-keyring

RUN apt install -y apt-transport-https dirmngr

RUN echo 'deb http://apt.dockerproject.org/repo debian-stretch main' >> /etc/apt/sources.list

COPY docker-gpg-key.asc /
RUN apt-key add /docker-gpg-key.asc

RUN apt update -y && apt install -y docker-engine

RUN echo 'Acquire::HTTP::Proxy "http://apt-cacher-ng:3142";' >> /etc/apt/apt.conf.d/01proxy \
 && echo 'Acquire::HTTPS::Proxy "false";' >> /etc/apt/apt.conf.d/01proxy

ADD entrypoint.sh aptinstalls.sh /

ADD install-grpc-cpp-plugin.sh /

COPY sudoers /

RUN groupadd -g 987 srbuser \
 && useradd -r -u 987 -g srbuser srbuser --home /home/srbuser \
 && adduser srbuser sudo \
 && adduser srbuser docker \
 && mkdir -p /home/srbuser/.ccache \
 && chown -R srbuser:srbuser /home/srbuser

ENV PATH /usr/lib/ccache:/usr/local/bin:/usr/local/sbin:$PATH

ENTRYPOINT ["/entrypoint.sh"]
