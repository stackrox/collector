#!/usr/bin/env python3

"""
Script to manage performance baseline metrics. Baseline is represented via json
array containing set of performance numbers from previous benchmark runs, and
is maintained as a json file on a GCS bucket. The only used data at the moment
is the benchmark length, stored in hackbench_avg_time field.

To add a new benchmark into the existing baseline, use --update <file.json>
command. This will fetch the existing series, add a new item into it and drop
the oldest items if the result is over the threshold. The final data will
overwrite existing baseline file on GCS.

To compare a new benchmark with the baseline, use --test <new.json> command.
This will fetch the baseline, and calculate an overhead captured in it and in
the test file (with/without collector). Then it will do a t-test for mean
values -- this will give the P-value, the probability of getting as or more
extreme values assuming the null hypothesis is true (i.e. the value we're
testing is different from the mean value from baseline only by chance). P-value
is distributed between [0, 1], bigger values means that the new benchmark
values differences from the baseline are not significant.

The procedure described above is a bit cheating though. While we're most likely
working with normally distributed values (hackbench_avg_time is an average, and
averages of samples are distributed normally following central theorem), the
test expects us to compare mean values, but we use a single sample as a second
argument. Essentially it means t-test answers the question "how likely it is
that the samples with mean M_0 has actual mean M_1", where M_0 is mean of the
baseline, and M_1 is the current value we're testing. How much is this problem
in practice needs to be verified empirically.
"""

import argparse
import json
import sys

from itertools import groupby
from scipy import stats

from google.cloud import storage


GCS_BUCKET = "stackrox-collector-benchmarks"
BASELINE_FILE = "baseline/all.json"
BASELINE_THRESHOLD = 5


def load_baseline_file():
    storage_client = storage.Client()

    bucket = storage_client.bucket(GCS_BUCKET)
    blob = bucket.blob(BASELINE_FILE)
    contents = blob.download_as_string()

    return json.loads(contents)


def add_to_baseline_file(input_file_name):
    storage_client = storage.Client()

    bucket = storage_client.bucket(GCS_BUCKET)
    blob = bucket.blob(BASELINE_FILE)
    contents = blob.download_as_string()

    data = json.loads(contents)
    with open(input_file_name, "r") as measure:
        result = data + json.load(measure)

        # Each benchmark data contains two values, with and without collector,
        # and the result array is a flattened version of it. The threshold is
        # formulated in benchmarks, so double it.
        if len(result) > BASELINE_THRESHOLD * 2:
            del result[0]
            del result[0]

        blob.upload_from_string(json.dumps(result))


def process(content):
    data = [
        record for record in content
        if record.get("Metrics", {}).get("hackbench_avg_time")
    ]

    processed = [
        {
            "kernel": record.get("VmConfig"),
            "collection_method": record.get("CollectionMethod"),
            record["TestName"]: record.get("Metrics").get("hackbench_avg_time")
        }
        for record in data
    ]

    def record_id(record):
        kernel = record.get("kernel")
        method = record.get("collection_method")
        return f"{kernel} {method}"

    def group_by(records, fn):
        ordered = sorted(records, key=fn)
        return groupby(ordered, key=fn)

    return group_by(processed, record_id)


def collector_overhead(measurements):
    empty = [
        m["baseline_benchmark"]
        for m in measurements
        if "baseline_benchmark" in m
    ]
    with_overhead = [
        m["collector_benchmark"]
        for m in measurements
        if "collector_benchmark" in m
    ]
    return [
        round(100 * x / y, 2)
        for (x, y) in zip(with_overhead, empty)
    ]


def compare(input_file_name):
    baseline_data = load_baseline_file()
    with open(input_file_name, "r") as measurement:
        test_data = json.load(measurement)
        baseline_groupped = process(baseline_data)
        test_groupped = process(test_data)

        for (baseline, test) in zip(baseline_groupped, test_groupped):
            bgroup, bvalues = baseline
            tgroup, tvalues = test

            baseline_overhead = collector_overhead(list(bvalues))
            test_overhead = collector_overhead(list(tvalues))[0]
            result, pvalue = stats.ttest_1samp(baseline_overhead,
                                               test_overhead)
            print(pvalue)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('--test', '-t',
                        help=('Input perf data for comparing with baseline.'))

    parser.add_argument('--update', '-u',
                        help=('Perf data to add to the baseline.'))

    args = parser.parse_args()

    if args.test:
        compare(args.test)
        sys.exit(0)

    if args.update:
        add_to_baseline_file(args.update)
        sys.exit(0)
