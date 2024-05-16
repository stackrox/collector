#!/usr/bin/env python3

import matplotlib
import matplotlib.pyplot as plt
import json
import argparse
import numpy as np
from scipy.stats import gaussian_kde


def load_file(filename):
    with open(filename, 'r') as inp:
        return json.load(inp)


def plot_kde(data, max, label):
    kde = gaussian_kde(data, bw_method=0.001)
    dist_space = np.linspace(0, 5000, 5000)

    plt.plot(dist_space, kde.pdf(dist_space), label=label)


def main(baseline_file, benchmark_file, syscall='read', output=None):
    if output:
        # use a backend that doesn't display, so we can write to file without
        # showing the figures
        matplotlib.use('Agg')

    baseline = load_file(baseline_file)
    benchmark = load_file(benchmark_file)

    syscall_baseline = sorted(baseline['raw'][syscall])
    syscall_benchmark = sorted(benchmark['raw'][syscall])

    plt.title(f"{syscall} latency".title())
    plt.xlabel("nanoseconds (ns)")
    plt.ylabel("probability")
    plt.gca().xaxis.grid(True)

    plot_kde(syscall_baseline, baseline['analysis'][syscall]['max'], "baseline")
    plot_kde(syscall_benchmark, benchmark['analysis'][syscall]['max'], "benchmark")

    plt.legend(("baseline", "with collector"), loc="best")

    if output:
        plt.savefig(output)
    else:
        plt.show()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('baseline')
    parser.add_argument('benchmark')
    parser.add_argument('--syscall')
    parser.add_argument('--output', '-o')

    args = parser.parse_args()

    syscall = args.syscall
    if not args.syscall:
        syscall = 'read'

    main(args.baseline, args.benchmark, syscall=syscall, output=args.output)
