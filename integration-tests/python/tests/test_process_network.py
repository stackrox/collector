import pytest
import time


@pytest.fixture
def nginx(runtime):
    nginx = runtime.run('nginx:1.14-alpine', name='nginx', remove=True)
    yield nginx
    nginx.kill()


def test_processes_recorded(runtime, collector, events, nginx):
    nginx.exec("ls -lah")
    nginx.exec("sleep 5")

    events.await_processes(container=nginx.id(), timeout=60)

    processes = events.processes(container=nginx.id())

    assert len(processes) == 4, f"Unexpected process count for container {nginx.id()}"

    for process in processes:
        print(process)
