import pytest
import time


@pytest.fixture(scope='function')
def nginx(runtime):
    nginx = runtime.run('nginx:1.14-alpine', name='nginx', remove=True)
    yield nginx
    nginx.kill()


@pytest.fixture(scope='function')
def curl(runtime):
    curl = runtime.run('curl', 'pstauffer/curl:latest', command='sleep 300', remove=True)
    yield curl
    curl.kill()


def test_processes_recorded(runtime, collector, events, nginx):
    nginx.exec("ls -lah")
    nginx.exec("sleep 5")

    events.await_processes(container=nginx.id(), timeout=60)

    processes = events.processes(container=nginx.id())

    assert len(processes) == 4, f"Unexpected process count for container {nginx.id()}"

    for process in processes:
        print(process)


def test_network_connections(runtime, collector, events, nginx, curl):
    curl.exec(f"curl {nginx.ip()}:{nginx.port()}")

    events.await_processes(container=curl.id(), timeout=60)

    network = events.network(container=nginx.id())
