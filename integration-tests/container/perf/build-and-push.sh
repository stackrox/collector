CONTAINERS=('bcc' 'init' 'perf' 'bpftrace')
REPO="${COLLECTOR_REPO:-stackrox}"

function container_name() {
    echo "${REPO}/collector-performance:${1}"
}

function build_container() {
    local DOCKER_FILE="Dockerfile.${1}"
    local NAME="$(container_name $1)"
    echo "[*] Building from ${DOCKER_FILE} -> ${NAME}"
    docker build -t "$(container_name $1)" -f "${DOCKER_FILE}" ${PWD}
}

DO_BUILD=1
DO_PUSH=1

case $1 in
    build)
        echo "[*] building containers"
        DO_BUILD=1
        DO_PUSH=0
        ;;
    push)
        echo "[*] /collectorpushing containers"
        DO_BUILD=0
        DO_PUSH=1
        ;;
    *)
        echo "[*] building and pushing containers"
        ;;
esac

for container in "${CONTAINERS[@]}"; do
    if [[ "$DO_BUILD" == "1" ]]; then
        build_container $container
    fi

    if [[ "$DO_PUSH" == "1" ]]; then
        echo "[*] pushing $(container_name $container)"
        docker push "$(container_name $container)"
    fi
done
