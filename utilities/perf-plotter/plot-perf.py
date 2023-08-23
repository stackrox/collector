#!/usr/bin/env python3

# Plot memory and CPU usage captured by the container-stats container.
#
# The script can be run interactively by downloading the benchmark artifact
# from GHA, extract it and run the following command:
#   jq -s 'flatten' perf.json | ./plot-perf.py -

import argparse
import json
import sys
import datetime as dt

import matplotlib.pyplot as plt
import matplotlib.dates as md


def bytes_to_float(bytes: str) -> float:
    """
    Transform a string representing bytes used by a process to a float

    Format of the input string is a floating point number followed by the
    scale in bytes, similar to these:
    - 0B
    - 15.63MiB
    """
    scale = bytes[-3:]

    if scale == 'GiB':
        return float(bytes[:-3]) * 1000
    if scale == 'MiB':
        return float(bytes[:-3])
    if scale == 'KiB':
        return float(bytes[:-3]) / 1000

    # If none of the previous matched, the number is most likely in the
    # format 12.34B
    return float(bytes[:-1]) / 1000000


def load_data(input) -> dict:
    """
    Load test data and reorganize collector specific data in format more
    convenient for processing
    """
    tests = json.load(input)

    return {
        test['VmConfig']: {
            test['CollectionMethod']: [
                cs for cs in test['ContainerStats'] if cs['Name'] == 'collector'
            ]
        } for test in tests
    }


def main(input, output: str):
    tests = load_data(input)

    fig, plots = plt.subplots(2, 1)
    cpuplot, memplot = plots

    # common configuration
    for plot in plots:
        plot.grid(True)
        # Make x axis show hours and minutes only
        plot.xaxis.set_major_formatter(md.DateFormatter('%H:%M'))

    for vm, tests in tests.items():
        for collection_method, test in tests.items():
            timestamp = [dt.datetime.strptime(t['Timestamp'], '%Y-%m-%d %H:%M:%S') for t in test]
            cpu = [t['Cpu'] for t in test]
            mem = [bytes_to_float(t['Mem']) for t in test]

            cpuplot.plot(timestamp, cpu, label=f'{vm}-{collection_method}')
            memplot.plot(timestamp, mem, label=f'{vm}-{collection_method}')

    # Create a single legend for both subplots
    handles, labels = memplot.get_legend_handles_labels()
    fig.legend(handles, labels, loc='lower right')
    fig.set_figheight(12)
    fig.set_figwidth(12)

    cpuplot.set_ylabel('CPU usage (%)')
    memplot.set_ylabel('Memory usage (MiB)')

    if output:
        plt.savefig(output)
    else:
        plt.show()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='File with performace data to be read. - reads from stdin')
    parser.add_argument('-o', '--output', help='Location to dump the plots', default='')

    args = parser.parse_args()

    input = sys.stdin if args.input == '-' else open(args.input, 'r')

    main(input, args.output)
