FROM centos:7

# Used to run the shellcheck-all.sh script, which checks for potential errors/smells in all shell script files in a directory
# To run 
# docker run --rm -v "$DIRECTORY_TO_SCAN:/scripts" shellcheck-all:latest

RUN yum update -y
RUN yum upgrade -y

RUN yum install -y wget xz-utils
RUN yum autoremove -y

RUN wget -qO- "https://github.com/koalaman/shellcheck/releases/download/stable/shellcheck-stable.linux.x86_64.tar.xz" | tar -xJv &&\
    cp "shellcheck-stable/shellcheck" /usr/bin/

COPY shellcheck-all.sh /usr/bin/shellcheck-all.sh

CMD /usr/bin/shellcheck-all.sh /scripts
