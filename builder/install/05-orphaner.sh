#!/usr/bin/env bash

set -e

make -C orphaner CFLAGS=-g LDFLAGS=-g install
