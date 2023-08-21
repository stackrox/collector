#!/usr/bin/env python3

import argparse
import json
import sys
import datetime as dt

import matplotlib.pyplot as plt
import matplotlib.dates as md


def bytes_to_float(bytes: str) -> float:
    """
    Transform a string representing bytes used by a process to a float
    """
    if bytes[-1] == 'B':
        bytes = bytes[:-1]

    if bytes[-1] == 'i':
        bytes = bytes[:-1]
        if bytes[-1] == 'M':
            return float(bytes[:-1])
        if bytes[-1] == 'K':
            return float(bytes[:-1]) / 1000
    else:
        return float(bytes) / 1000000


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

    if len(output) == 0:
        plt.show()
    else:
        plt.savefig(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='File with performace data to be read. - reads from stdin')
    parser.add_argument('-o', '--output', help='Location to dump the plots', default='')

    args = parser.parse_args()

    input = sys.stdin if args.input == '-' else open(args.input, 'r')

    main(input, args.output)
