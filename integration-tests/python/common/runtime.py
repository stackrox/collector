import os
from abc import ABC, abstractmethod

import docker
from docker import DockerClient

from common.container import DockerContainer


class Runtime(ABC):
    @abstractmethod
    def run(self, image, name=None, command=None, env=None, mounts=None, *args, **kwargs):
        pass


class DockerRuntime(Runtime):
    def __init__(self):
        self.client: DockerClient = docker.from_env()

        username = os.environ.get('QUAY_USERNAME')
        password = os.environ.get('QUAY_PASSWORD')

        self.client.login(
            username=username,
            password=password,
            registry='quay.io'
        )

    def run(self, image, name=None, command=None, env=None, mounts=None, *args, **kwargs):
        handle = self.client.containers.run(
            image,
            command,
            name=name,
            mounts=mounts,
            environment=env,
            # always detach to ensure that the API returns a proper
            # container object (otherwise we get stdout/stderr)
            detach=True,
            *args,
            **kwargs
        )

        return DockerContainer(handle)
