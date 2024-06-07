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

    def __len__(self):
        return len(self.tasks)

    def __repr__(self):
        return f'{self.name}: {len(self)}'

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
        os.makedirs(self.output_dir, exist_ok=True)
        self._dump_all()

    def match(self, task):
        return self.regex.match(task.kernel) is not None

    def append(self, task):
        if task.kernel not in self.tasks:
            self.tasks[task.kernel] = [(task.module, task.driver_type)]
        else:
            self.tasks[task.kernel].append((task.module, task.driver_type))

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
    oracle_kernels = r"(?:.*el7uek.*)"
    fc39_kernels = r"(?:(?:6\.[1-9]\..*)|(?:6\.[1-9][0-9]\..*)|(:?[7-9]\.\d+\..*))"
    fc36_kernels = r"(?:(?:5\.[1-9]\d*\..*)|(?:6\.[0-5]\..*))"
    rhel9_kernels = r"(?:(?:4|5)\.\d+\..*)"
    rhel7_kernels = r"(?:3\.\d+\..*)"
    rhel7_ebpf_kernels = r"(?:(?:3|4|5)\.\d+\..*)"

    oracle = Builder("oracle", rf"^{oracle_kernels}", {})
    fc39 = Builder("fc39", rf"^{fc39_kernels}", {})
    fc36 = Builder("fc36", rf"^{fc36_kernels}", {})
    rhel7_ebpf = EBPFBuilder("rhel7", rf"^{rhel7_ebpf_kernels}", {})
    rhel9 = Builder("rhel9", rf"^{rhel9_kernels}", {})
    rhel7 = Builder("rhel7", rf"^{rhel7_kernels}", {})
    unknown = Builder("unknown", r".*", {})

    builders = [
        oracle,
        fc39,
        fc36,
        rhel9,
        rhel7_ebpf,
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

    driver_builders = int(os.environ.get('DRIVER_BUILDERS', 1))

    oracle_builders = oracle.split(driver_builders)
    rhel9_builders = rhel9.split(driver_builders)
    rhel7_ebpf_builders = rhel7_ebpf.split(driver_builders)
    rhel7_builders = rhel7.split(driver_builders)
    fc36_builders = fc36.split(driver_builders)
    fc39_builders = fc39.split(driver_builders)

    builders = [
        *oracle_builders,
        *fc39_builders,
        *fc36_builders,
        *rhel9_builders,
        *rhel7_ebpf_builders,
        *rhel7_builders,
        unknown
    ]

    for builder in builders:
        builder.dump()


if __name__ == "__main__":
    with open(os.path.join(TASKS_DIR, 'all'), 'r') as tasks:
        main(tasks)
