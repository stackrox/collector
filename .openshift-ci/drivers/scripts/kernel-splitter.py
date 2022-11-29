#! /usr/bin/env python3

import os
import re
from math import ceil
from itertools import islice

TASKS_DIR = os.environ.get('TASKS_DIR', '/tasks')


class Task:
    def __init__(self, kernel, module, driver_type):
        self.kernel = kernel
        self.module = module
        self.driver_type = driver_type

    def parse(task_line: str):
        task = task_line.strip().split()

        if len(task) != 3:
            # invalid line
            return None

        return Task(task[0], task[1], task[2])

    def is_ebpf(self):
        return self.driver_type == "bpf"

    def is_kernel_module(self):
        return self.driver_type == "mod"


class Builder:
    def __init__(self, name, regex, tasks):
        self.name = name
        self.output_dir = os.path.join(TASKS_DIR, name)
        self.regex = re.compile(regex)
        self.tasks = tasks
        # OSCI caps cpu cores to 10, so that's what each builder gets
        self._shards = 10

    def __len__(self):
        return len(self.tasks)

    @property
    def shards(self):
        return self._shards

    @shards.setter
    def shards(self, s):
        self._shards = s if s > 0 else 1

    def __repr__(self):
        return f'{self.name}: {len(self)}'

    def _dump_shard(self, shard, tasks):
        output_file = os.path.join(self.output_dir, 'shards', str(shard))
        with open(output_file, 'a+') as f:
            f.writelines(tasks)

    def _dump_all(self):
        raw_tasks = [
            f'{kernel} {module} {driver_type}\n'
            for kernel, value in self.tasks.items()
            for module, driver_type in value
        ]

        with open(os.path.join(self.output_dir, 'all'), 'a+') as f:
            f.writelines(raw_tasks)

    def dump(self):
        # Create the directory for storing tasks and fill the 'all' tasks file
        os.makedirs(os.path.join(self.output_dir, 'shards'), exist_ok=True)

        self._dump_all()

        shard = 0
        raw_tasks = []
        for i, (kernel, value) in enumerate(self.tasks.items()):
            raw_tasks.extend([
                f'{kernel} {module} {driver_type}\n'
                for module, driver_type in value
            ])

            if i < self.get_tasks_per_shard() * (shard + 1):
                continue

            # filled up the shard, dump it and move on to the next one
            self._dump_shard(shard, raw_tasks)

            shard += 1
            raw_tasks = []

        # dump the remaining kernels
        if len(raw_tasks):
            self._dump_shard(shard, raw_tasks)

    def match(self, task):
        return self.regex.match(task.kernel) is not None

    def append(self, task):
        if task.kernel not in self.tasks:
            self.tasks[task.kernel] = [(task.module, task.driver_type)]
        else:
            self.tasks[task.kernel].append((task.module, task.driver_type))

    def get_tasks_per_shard(self):
        if self.shards == 0:
            return -1

        return len(self.tasks) / self.shards

    def split(self, new_builders: int) -> list:
        """
        Will split tasks among new builders and return a list holding those
        builders.

        Parameters:
            new_builders (int): the amount of builders the tasks should be split into

        Returns:
            The list of builders ready to be dumped
        """
        if len(self) == 0:
            return []

        chunk_size = ceil(len(self) / new_builders)

        new_builders_tasks = []
        for chunk in range(0, len(self), chunk_size):
            new_builders_tasks.append({
                task: self.tasks[task]
                for task in islice(self.tasks, chunk, chunk + chunk_size)
            })

        return [
            Builder(f'{self.name}/{i}', self.regex, tasks)
            for i, tasks in enumerate(new_builders_tasks)
        ]


class EBPFBuilder(Builder):
    def match(self, task):
        return self.regex.match(task.kernel) and task.is_ebpf()


class ModBuilder(Builder):
    def match(self, task):
        return self.regex.match(task.kernel) and task.is_kernel_module()


def main(task_file):
    fc36_kernels = r"(?:(?:5\.[1-9]\d+\..*)|(:?[6-9]\.\d+\..*))"
    rhel8_kernels = r"(?:(?:4|5)\.\d+\..*)"
    rhel7_kernels = r"(?:3\.\d+\..*)"
    rhel7_ebpf_kernels = r"(?:(?:3|4|5)\.\d+\..*)"

    fc36 = Builder("fc36", rf"^{fc36_kernels}", {})
    rhel7_ebpf = EBPFBuilder("rhel7", rf"^{rhel7_ebpf_kernels}", {})
    rhel8 = Builder("rhel8", rf"^{rhel8_kernels}", {})
    rhel7 = Builder("rhel7", rf"^{rhel7_kernels}", {})
    unknown = Builder("unknown", r".*", {})

    builders = [
        fc36,
        rhel7_ebpf,
        rhel8,
        rhel7
    ]

    # Order is important in this for loop!
    # Kernels will be assigned to the first builder they match with, keep it in
    # mind when writing or dealing with overlapping regexes.
    for line in task_file.readlines():
        matched = False
        task = Task.parse(line)

        if task is None:
            print(f'Failed to parse line "{line.strip()}"')
            continue

        for builder in builders:
            if builder.match(task):
                builder.append(task)
                matched = True
                break

        if not matched:
            print(f'No builder for "{line.strip()}"')
            unknown.append(task)

    # We split the rhel 8 builder to make better usage of OSCI
    rhel8_builders_count = int(os.environ.get('RHEL8_BUILDERS', 4))
    rhel8_builders = rhel8.split(rhel8_builders_count)

    rhel7_ebpf_builders = rhel7_ebpf.split(rhel8_builders_count)
    rhel7_builders = rhel7.split(rhel8_builders_count)

    fc36_builders_count = int(os.environ.get('RHEL8_BUILDERS', 1))
    fc36_builders = fc36.split(fc36_builders_count)

    builders = [
        *fc36_builders,
        *rhel8_builders,
        *rhel7_ebpf_builders,
        *rhel7_builders,
        unknown
    ]
    for builder in builders:
        builder.dump()


if __name__ == "__main__":
    with open(os.path.join(TASKS_DIR, 'all'), 'r') as tasks:
        main(tasks)
