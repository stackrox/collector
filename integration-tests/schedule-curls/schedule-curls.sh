#!/usr/bin/env sh

num_meta_iter=$1
num_iter=$2
sleep_between_curl_time=$3
sleep_between_iterations=$4
url=$5

i=0
j=0

while [ "$i" -lt "$num_meta_iter" ]; do
    while [ "$j" -lt "$num_iter" ]; do
        curl "$url"
        sleep "$sleep_between_curl_time"
        j=$((j + 1))
    done
    sleep "$sleep_between_iterations"
    i=$((i + 1))
done
