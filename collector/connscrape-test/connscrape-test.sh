#!/usr/bin/env bash
set -eo pipefail

#nohup wget http://www.nytimes.com &
nohup nc -l &
/tmp/collector/cmake-build/collector/connscrape > /tmp/connscrape_log.txt
/tmp/collector/collector/connscrape-test/test-connscrape-log.sh /tmp/connscrape_log.txt
ls /proc
