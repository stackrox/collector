#!/usr/bin/env bash

prefix="${1:-}"
suffix="${2:-}"

awk -v prefix="$prefix" -v suffix="$suffix" '{ print prefix $0 suffix }'
