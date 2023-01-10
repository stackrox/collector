import time
from abc import ABC, abstractmethod
from collections import defaultdict

from boltdb import BoltDB

from common.process import Process

class Events(ABC):
    @abstractmethod
    def processes(self):
        pass

    @abstractmethod
    def network(self):
        pass

class BoltDBEvents(Events):
    def __init__(self):
        # lazy load the DB on demand, to avoid
        # weird behavior when opening the DB really 
        # early in the test
        self.db = None

    def await_processes(self, container, timeout=30):
        for _ in range(timeout):
            with self.open().view() as tx:
                process_bucket = tx.bucket(b'Process')
                if process_bucket:
                    container_bucket = process_bucket.bucket(container.encode())
                    if container_bucket:
                        return
                    
            time.sleep(1)

        raise RuntimeError(f'Timed out waiting for processes for {container}')

    def processes(self, container=None):
        with self.open().view() as tx:
            process_bucket = tx.bucket(b'Process')

            if not process_bucket:
                return []

            processes = defaultdict(list)

            for container_id, _ in process_bucket:
                container_bucket = process_bucket.bucket(container_id)

                container_id = container_id.decode()
                if container is not None and container_id != container:
                    continue

                for key, value in container_bucket:
                    processes[container_id].append(Process(value.decode()))

            return processes if not container else processes.get(container, [])

    def network(self):
        pass

    def open(self):
        if not self.db:
            self.db = BoltDB('/tmp/collector-test.db', readonly=True)
        return self.db
