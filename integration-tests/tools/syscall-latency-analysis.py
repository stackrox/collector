import tabulate
import argparse
import sys
import json
import os

g_collector_syscalls = [
    "accept",
    "chdir",
    "clone",
    "close",
    "connect",
    "execve",
    "fchdir",
    "fork",
    "procexit",
    "procinfo",
    "setresgid",
    "setresuid",
    "setgid",
    "setuid",
    "shutdown",
    "socket",
    "vfork",
]


def p95(data):
    """
    :return: the 95th percentile value from the data set
    """
    idx = round(len(data) * 0.95)
    if idx == len(data):
        idx = idx - 1
    return data[idx]


def mean(data):
    """
    :return: the mean average from the set of data
    """
    return sum(data) / len(data)


def median(data):
    """
    :return: the middle value in the set of data
    """
    return data[len(data) // 2]


def results_file(root, kind, version, collection=None):
    """
    :param root: the root directory of the data files
    :param kind: the kind of measurement (baseline or benchmark)
    :param version: the version of collector used to generate the data
    :param collection: the kind of collection used (kernel-module or ebpf)

    :return: the file path of the expected json data file
    """
    return os.path.join(root, f'{f"{collection}-" if collection else ""}{kind}-{version}.json')

def load_dataset(root, name, version, collection=None):
    """
    :param root: the root directory of the data files
    :param kind: the kind of measurement (baseline or benchmark)
    :param version: the version of collector used to generate the data
    :param collection: the kind of collection used (kernel-module or ebpf)

    :return: a dictionary containing the loaded data set
    """
    filename = results_file(root, name, version, collection=collection)
    print(f"[*] loading {filename}")
    return json.load(open(filename))


def process(data_set):
    """
    Goes through the data_set to process mean, median, and p95 values
    for all syscalls in the set. 

    :return: a new dictionary containing the processed results
    """
    results = {}
    for key, values in data_set.items():
        values = sorted(values)
        results[key] = {
            'mean': mean(values),
            'median': median(values),
            'p95': p95(values),
            'size': len(values),
        }

    return results

def display(baseline_results, ebpf_results, ko_results):
    headers = ["syscall", "baseline (mean/median/p95)", "ebpf (mean/median/p95)", "ko (mean/median/p95)"]
    table = []

    for key in sorted(baseline_results.keys(), key=lambda a: len(baseline[a])):
        try:
            base = baseline_results[key]
            ebpf = ebpf_results[key]
            ko = ko_results[key]
        except KeyError:
            print(f"[*] failed to find data in all sets for {key}")
            continue

        table.append([
            f"{key} ({len(baseline[key])})",
            f"{base['mean']:.2f}\n{base['median']}\n{base['p95']}",
            f"{ebpf['mean']:.2f}\n{ebpf['median']}\n{ebpf['p95']}",
            f"{ko['mean']:.2f}\n{ko['median']}\n{ko['p95']}",
        ])

    print(tabulate.tabulate(table, headers=headers))

g_epilog = """
This tool processes json files that contain syscall latency information of the form:
    {
        "<syscall name/number>": [<list of latency valules>],
        ...
    }
It expects three files, one for the baseline, one for ebpf collection and one for kernel-module
collection e.g.

    baseline-3.6.0.json
    ebpf-benchmark-3.6.0.json
    kernel-module-benchmark-3.6.0.json
"""

if __name__ == '__main__':
    parser = argparse.ArgumentParser(epilog=g_epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('version', help='the collector version')
    parser.add_argument(
        'data_root', help='The root directory containing the results data')

    args = parser.parse_args()

    print(f"[*] processing {args.version} data in {args.data_root}.")

    baseline = load_dataset(args.data_root, "baseline", args.version)
    ebpf = load_dataset(args.data_root, "benchmark", args.version, collection="ebpf")
    ko = load_dataset(args.data_root, "benchmark", args.version, collection="kernel-module")

    print("[*] processing baseline latencies")
    baseline_results = process(baseline)
    print("[*] processing ebpf latencies")
    ebpf_results = process(ebpf)
    print("[*] processing kernel module latencies")
    ko_results = process(ko)

    display(baseline_results, ebpf_results, ko_results)

    

