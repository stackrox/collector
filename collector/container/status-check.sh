#!/usr/bin/env bash

# /ready API will return the following formatted response:
# {
#    "collector" : {
#       "drops" : 0,
#       "events" : 9330070,
#       "node" : "node.name",
#       "preemptions" : 0
#    },
#    "status" : "ok"
# }
#
# Take the status line, split it by ":" and trim spaces and quotes.
STATUS=$(curl -s localhost:8080/ready | grep 'status' | awk -F ':' '{print $2}' | xargs)

if [[ "${STATUS}" = "ok" ]]; then
    exit 0
else
    exit 1
fi
