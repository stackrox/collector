import docker
import pytest
import os
import json

from docker.client import DockerClient
from docker.types import Mount

from common.container import Container


@pytest.fixture(scope="session", autouse=True)
def docker_client() -> DockerClient:
    """
    Creates a Docker client

    @return client that can be used to interact with docker
    """
    return docker.from_env()


@pytest.fixture(scope="function")
def grpc_server(docker_client: DockerClient) -> Container:

    grpc_server = docker_client.containers.run(
        "quay.io/stackrox-io/grpc-server:3.72.x-281-g25a7abf818",
        name="grpc-server",
        remove=True,
        mounts=[Mount("/tmp", "/tmp", type="bind")],
        network="host",
        detach=True,
        user=os.getuid(),
    )

    yield grpc_server

    with open("/tmp/grpc.log", "w") as log:
        log.write(grpc_server.logs().decode())

    try:
        grpc_server.kill()
    except Exception:
        pass


@pytest.fixture(scope="function")
def collector(
    request, docker_client: DockerClient, grpc_server: Container
) -> Container:
    params = getattr(request, "param", {})
    assert isinstance(params, dict)

    configuration = {"logLevel": "debug", "turnOffScrape": True, "scrapeInterval": 2}

    configuration.update(params.get("config", {}))

    env = {
        "COLLECTOR_PRE_ARGUMENTS": os.environ.get("COLLECTOR_PRE_ARGUMENTS", ""),
        "COLLECTION_METHOD": os.environ.get("COLLECTION_METHOD", "ebpf").replace(
            "-", "_"
        ),
        "GRPC_SERVER": "localhost:9999",
        "ENABLE_CORE_DUMP": "false",
        # remove 'space' from the separators to ensure it is a valid environment
        # variable.
        "COLLECTOR_CONFIG": json.dumps(configuration, separators=(",", ":")),
        "MODULE_DOWNLOAD_BASE_URL": "https://collector-modules.stackrox.io/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656",
    }

    env.update(params.get("env", {}))

    # strictly speaking this can be a list, but by using a dictionary
    # keyed by host path, the tests can override individual mounts
    # to test collector's behavior in certain situations
    mounts = {
        "/var/run/docker.sock": Mount(
            "/host/var/run/docker.sock",
            "/var/run/docker.sock",
            read_only=True,
            type="bind",
        ),
        "/proc": Mount("/host/proc", "/proc/", read_only=True, type="bind"),
        "/etc": Mount("/host/etc", "/etc/", read_only=True, type="bind"),
        "/usr/lib": Mount("/host/usr/lib", "/usr/lib/", read_only=True, type="bind"),
        "/sys": Mount("/host/sys", "/sys/", read_only=True, type="bind"),
        "/dev": Mount("/host/dev", "/dev/", read_only=True, type="bind"),
        "/tmp": Mount("/tmp", "/tmp", type="bind"),
        # /module is an anonymous volume to reflect the way collector
        # is usually run in kubernetes (with in-memory volume for /module)
        "/module": Mount("/module", None, type="tmpfs"),
    }

    mounts.update(params.get("mounts", {}))

    collector_image = "quay.io/stackrox-io/collector:3.12.0"

    collector = docker_client.containers.run(
        collector_image,
        detach=True,
        name="collector",
        privileged=True,
        network="host",
        mounts=list(mounts.values()),
        environment=env,
    )

    yield collector

    try:
        collector.kill()
    except Exception:
        pass

    with open("/tmp/collector.log", "w") as log:
        log.write(collector.logs().decode())

    collector.remove()


@pytest.fixture(scope="function")
def db_path() -> str:
    return "/tmp/collector-test.db"
