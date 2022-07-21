#! /usr/bin/env python3

import os
import re
from math import floor

TASKS_DIR = '/tasks'


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


class Builder:
    def __init__(self, name, regex):
        self.output_dir = os.path.join(TASKS_DIR, name)
        self.regex = re.compile(regex)
        self.tasks = {}
        self._shards = 0

    def __len__(self):
        return len(self.tasks)

    @property
    def shards(self):
        return self._shards

    @shards.setter
    def shards(self, s):
        self._shards = s if s > 0 else 1

    def _dump_shard(self, shard, tasks):
        output_file = os.path.join(self.output_dir, 'shards', str(shard))
        with open(output_file, 'w') as f:
            f.writelines(tasks)

    def _dump_all(self):
        raw_tasks = [
            f'{kernel} {module} {driver_type}\n'
            for kernel, value in self.tasks.items()
            for module, driver_type in value
        ]

        with open(os.path.join(self.output_dir, 'all'), 'w') as f:
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


def distribute_shards(total_shards, builders):
    total_tasks = 0
    used_shards = 0

    total_tasks = sum(len(b) for b in builders)

    if total_tasks == 0:
        # No drivers left to build
        return

    for builder in builders:
        builder.shards = int(floor((len(builder) * total_shards) / total_tasks))
        used_shards += builder.shards

    while used_shards < total_shards:
        min_builder = min(builders, key=lambda b: b.get_tasks_per_shard())

        min_builder.shards += 1
        used_shards += 1


def main(task_file):
    # suse4_kernels = r'(?:4\.\d+\.\d+-\d+\.\d+-default)'
    fc36_kernels = r'(?:5\.[1-9]\d+\..*)'
    rhel8_kernels = r'(?:(?:4|5)\.\d+\..*)'
    rhel7_kernels = r'(?:3\.\d+\..*)'

    fc36 = Builder('fc36', fr'^{fc36_kernels}')
    rhel8 = Builder('rhel8', fr'^{rhel8_kernels}')
    rhel7 = Builder('rhel7', fr'^{rhel7_kernels}')
    unknown = Builder('unknown', r'.*')

    builders = [
        fc36,
        rhel8,
        rhel7,
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

    # Every builder will use parallel builds, figure out how many shards each of
    # them will use
    total_shards = int(os.environ.get('MAX_PARALLEL_BUILDS', 3))
    distribute_shards(total_shards, builders)

    for builder in builders:
        builder.dump()

    # Dump kernels using an unknown builder
    unknown.dump()


if __name__ == "__main__":
    with open(os.path.join(TASKS_DIR, 'all'), 'r') as tasks:
        main(tasks)
