import logging
from subprocess import CalledProcessException

from collector_tools import shell


class Instance:
    def __init__(self, name, wrapper):
        self._wrapper = wrapper
        self.name = name

    def ssh(self, command, *args, username=None):
        return self._wrapper.ssh(self.name, command, *args, username=username)

    def scp(self, source, destination, *args, username=None):
        return self._wrapper.scp(self.name, source, destination, *args, username=username)

    def __del__(self):
        self._wrapper.delete_instance(self.name)


class Compute:
    def __init__(self, project, zone="us-central1-a", service_account=None):
        self.project = project
        self.zone = zone
        self.service_account = service_account

        self._default_args = [f"--project={self.project}", f"--zone={self.zone}"]

    def create_instance(
        self, name, image_project, image_family, *args, machine_type="e2-standard-2"
    ):
        args = [
            f"--image-family={image_family}",
            f"--image-project={image_project}",
            f"--machine-type={machine_type}",
        ] + args

        if self.service_account:
            args.append(f"--service-account={self.service_account}")

        args.append(name)
        try:
            self.instances("create", *args)
        except CalledProcessException as e:
            logging.error(f"failed to create instance {name}: {e}")

        return Instance(name, self)

    def delete_instance(self, name, *args):
        return self.instances("delete", *args, name)

    def ssh(self, instance, command, *args, username=None):
        if username:
            instance = f"{username}@{instance}"
        args.append(instance)
        return self.compute("ssh", *args)

    def scp(self, instance, source, destination, *args, username=None):
        if username:
            source = f"{username}@{instance}:{source}"

        args = args + [source, destination]
        return self.compute("scp", *args)

    def instances(self, command, *args):
        args = self._default_args + args
        return self.compute("instances", command, *args)

    def compute(self, command, *args):
        args = self._default_args + args
        return self._run("compute", command, *args)

    def _run(self, *args):
        return shell.cmd("gcloud", *args)
