#!/bin/sh

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
# Pattern match for "status":"ok" in the JSON response
case "$(curl -sf localhost:8080/ready)" in
    *'"status"'*'"ok"'*) exit 0 ;;
    *) exit 1 ;;
esac
