#!/usr/bin/env bash

connscrape_log=$1

grep Endpoints: "$connscrape_log" 
grep Endpoints: -A1 "$connscrape_log" | grep "0.0.0.0:31337"
