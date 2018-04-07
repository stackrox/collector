#!/bin/bash

DOCKER_SOCK='unix:///host/var/run/docker.sock'
DOCKER=(/usr/local/bin/docker)
CONTAINER_ID=$("${DOCKER[@]}" inspect --format '{{.Id}}' $HOSTNAME)
NODE=$("${DOCKER[@]}" info -f '{{.Name}}')
NAME="${COLLECTOR_NAME}-${NODE}-priv"
IMAGE=$("${DOCKER[@]}" inspect --format '{{.Config.Image}}' $HOSTNAME)

cleanup() {
    "${DOCKER[@]}" stop "$NAME"
}

trap cleanup EXIT

## In rare cases, the automatic garbage collection of the subcontainer
## that should happen because we use run --rm is too slow for us, so
## make sure any previous subcontainer is removed
EXISTING=$("${DOCKER[@]}" ps -q -f name="$NAME")
if [ -n "$EXISTING" ]; then
    "${DOCKER[@]}" rm -fv "$EXISTING" || true
fi

"${DOCKER[@]}" run --privileged --rm --name $NAME \
        $COLLECTOR_ENVS --network=container:$CONTAINER_ID \
        $COLLECTOR_MOUNTS $COLLECTOR_LABELS \
        --log-driver='json-file' --log-opt='max-size=1m' --log-opt='max-file=10' \
        --cpu-period=100000 --cpu-quota=$ROX_COLLECTOR_CPU_QUOTA \
        --restart=no $IMAGE
