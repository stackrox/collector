#!/usr/bin/env sh


while true
do 
  docker stats --no-stream -a | \
    grep -E '(benchmark|collector|grpc-server)' | \
    awk '{gsub(/%$/,"",$3); printf("{\"timestamp\": \"%s\",\"id\": \"%s\", \"name\": \"%s\", \"mem\": \"%s\", \"cpu\": %s}\n", strftime("%Y-%m-%d %H:%M:%S", systime(), 1), $1, $2, $4, $3 ); fflush();}'
  sleep 1
done
