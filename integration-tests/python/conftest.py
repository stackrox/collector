import docker
import pytest
import os
import json
import time

from docker.client import DockerClient
from docker.types import Mount

from common.container import Container
from common.runtime import Runtime, DockerRuntime
from common.events import Events, BoltDBEvents

from boltdb import BoltDB


@pytest.fixture(scope="session", autouse=True)
def runtime() -> Runtime:
    """
    Creates a container runtime based on the test configuration
    """
    return DockerRuntime()

@pytest.fixture(scope="function")
def grpc_server(runtime: Runtime) -> Container:

    grpc_server = runtime.run(
        "quay.io/rhacs-eng/grpc-server:3.72.x-281-g25a7abf818",
        name="grpc-server",
        remove=True,
        mounts=[Mount("/tmp", "/tmp", type="bind")],
        network="host",
        user=os.getuid(),
    )

    yield grpc_server

    with open("/tmp/grpc.log", "w") as log:
        log.write(grpc_server.logs())

    try:
        grpc_server.kill()
    except Exception:
        pass


@pytest.fixture(scope="function")
def collector(
    request, runtime: Runtime, grpc_server: Container
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

    collector = runtime.run(
        collector_image,
        name="collector",
        privileged=True,
        network="host",
        mounts=list(mounts.values()),
        env=env,
    )

    time.sleep(30)

    yield collector

    try:
        collector.kill()
    except Exception:
        pass

    with open("/tmp/collector.log", "w") as log:
        log.write(collector.logs())

    collector.remove()


@pytest.fixture(scope="function")
def events() -> Events:
    yield BoltDBEvents()

    os.remove('/tmp/collector-test.db')
