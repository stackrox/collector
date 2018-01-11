#!/usr/bin/env sh
/kernel-crawler.py $1 > unsorted
cat unsorted | rev | sort | rev
