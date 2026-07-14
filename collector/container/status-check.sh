#!/usr/bin/env bash

# Pattern-matches the JSON response rather than just checking HTTP status,
# confirming the collector event loop is actually running (not just the HTTP
# server). Uses shell glob matching since jq is not available in UBI-micro.

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
# -s: suppress progress noise; -f: non-zero exit on HTTP errors.
# Port 8080 is the civetweb introspection server (not Prometheus on 9090).
case "$(curl -sf localhost:8080/ready)" in
    *'"status"'*'"ok"'*) exit 0 ;;
    *) exit 1 ;;
esac
