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
import os
import sys

from itertools import groupby, zip_longest
from operator import itemgetter
from collections import Counter
from scipy import stats

from google.oauth2 import service_account
from google.cloud import storage
from google.api_core.exceptions import NotFound


DEFAULT_GCS_BUCKET = "stackrox-ci-results"
DEFAULT_BASELINE_FILE = "circleci/collector/baseline/all.json"
DEFAULT_BASELINE_THRESHOLD = 5


def load_baseline_file(bucket, baseline_file):
    credentials = json.loads(os.environ["GOOGLE_CREDENTIALS_CIRCLECI_COLLECTOR"])
    storage_credentials = service_account.Credentials.from_service_account_info(credentials)
    storage_client = storage.Client(credentials=storage_credentials)

    bucket = storage_client.bucket(bucket)
    blob = bucket.blob(baseline_file)

    try:
        contents = blob.download_as_string()
        return json.loads(contents)

    except NotFound as ex:
        print(f"File gs://{GCS_BUCKET}/{BASELINE_FILE} not found. "
               "Creating a new empty one.", file=sys.stderr)

        blob.upload_from_string(json.dumps([]))
        return []


def save_baseline_file(bucket, baseline_file, data):
    credentials = json.loads(os.environ["GOOGLE_CREDENTIALS_CIRCLECI_COLLECTOR"])
    storage_credentials = service_account.Credentials.from_service_account_info(credentials)
    storage_client = storage.Client(credentials=storage_credentials)

    bucket = storage_client.bucket(bucket)
    blob = bucket.blob(baseline_file)
    blob.upload_from_string(json.dumps(data))


def is_no_overhead(record):
    return record.get("TestName") == "baseline_benchmark"


def is_overhead(record):
    return record.get("TestName") == "collector_benchmark"


def add_to_baseline_file(input_file_name, data, threshold):
    with open(input_file_name, "r") as measure:
        new_measurement = json.load(measure)

        if data:
            verify_new_data(data, new_measurement)

        result = []
        for _, values in group_data(data, "VmConfig", "CollectionMethod"):
            ordered = sorted(values,
                    key=itemgetter("Timestamp"), reverse=True)

            # Each benchmark data contains two values, with and without
            # collector, and the result array is a flattened version of it. The
            # threshold is formulated in benchmarks, so double it.
            if len(ordered) >= threshold * 2:
                # Drop one oldest pair (no overhead, overhead) in every group
                _, *no_overhead = filter(is_no_overhead, ordered)
                _, *overhead = filter(is_overhead, ordered)
            else:
                no_overhead = [*filter(is_no_overhead, ordered)]
                overhead = [*filter(is_overhead, ordered)]

            result.extend(no_overhead + overhead)

        result.extend(new_measurement)
        return result


"""
Enforce compatibility between baseline data and a new chunk of numbers. Two
invariants needs to be maintained:

* both baseline and new data have the same set of (VmConfig, CollectionMethod)

* New data provides equal number of with/without collector benchmark runs

The function expects baseline is not empty.

"""
def verify_new_data(baseline_data, new_data):
    assert baseline_data, "Baseline data must be not empty"

    baseline_groupped = process(baseline_data)
    new_groupped = process(new_data)

    for (baseline, new) in zip_longest(baseline_groupped, new_groupped):
        bgroup, bvalues = baseline
        ngroup, nvalues = new

        if bgroup != ngroup:
            raise Exception(f"Benchmark metrics do not match:"
                             " {bgroup}, {ngroup} ")

        counter = Counter()
        for v in nvalues:
            counter.update(v.keys())

        no_overhead_count = counter.get("baseline_benchmark", 0)
        overhead_count = counter.get("collector_benchmark", 0)

        if (no_overhead_count != overhead_count):
            raise Exception(f"Number of with/without overhead do not match:"
                             " {no_overhead_count}, {overhead_count} ")


"""
Transform benchmark data into the format CI scripts work with.
"""
def process(content):
    data = [
        record for record in content
        if record.get("Metrics", {}).get("hackbench_avg_time")
    ]

    processed = [
        {
            "kernel": record.get("VmConfig"),
            "collection_method": record.get("CollectionMethod"),
            "timestamp": record.get("Timestamp"),
            record["TestName"]: record.get("Metrics").get("hackbench_avg_time")
        }
        for record in data
    ]

    return group_data(processed, "kernel", "collection_method")


def group_data(content, *columns):
    def record_id(record):
        return " ".join([record.get(c) for c in columns])

    def group_by(records, fn):
        ordered = sorted(records, key=fn)
        return groupby(ordered, key=fn)

    return group_by(content, record_id)


def collector_overhead(measurements):
    no_overhead = [
        m["baseline_benchmark"]
        for m in measurements
        if "baseline_benchmark" in m
    ]
    overhead = [
        m["collector_benchmark"]
        for m in measurements
        if "collector_benchmark" in m
    ]
    return [
        round(100 * x / y, 2)
        for (x, y) in zip(overhead, no_overhead)
    ]


def compare(input_file_name, baseline_data):
    if not baseline_data:
        print(f"Baseline file is empty, nothing to compare.", file=sys.stderr)
        return

    with open(input_file_name, "r") as measurement:
        test_data = json.load(measurement)
        verify_new_data(baseline_data, test_data)

        baseline_groupped = process(baseline_data)
        test_groupped = process(test_data)

        for (baseline, test) in zip(baseline_groupped, test_groupped):
            bgroup, bvalues = baseline
            tgroup, tvalues = test

            assert bgroup == tgroup, "Kernel/Method must not be differrent"

            baseline_overhead = collector_overhead(list(bvalues))
            test_overhead = collector_overhead(list(tvalues))[0]
            result, pvalue = stats.ttest_1samp(baseline_overhead,
                                               test_overhead)
            print(f"{bgroup} {pvalue}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('--test', '-t',
                        help=('Input perf data for comparing with baseline.'))

    parser.add_argument('--update', '-u',
                        help=('Perf data to add to the baseline.'))

    parser.add_argument('--gcs-bucket', nargs='?', default=DEFAULT_GCS_BUCKET,
                        help=('GCS bucket to store the data'))

    parser.add_argument('--baseline-file', nargs='?',
                        default=DEFAULT_BASELINE_FILE,
                        help=('The file containing baseline data'))

    parser.add_argument('--baseline-threshold', nargs='?',
                        default=DEFAULT_BASELINE_THRESHOLD,
                        help=('Maximum number of benchmark runs in baseline'))

    args = parser.parse_args()

    if args.test:
        baseline_data = load_baseline_file(args.gcs_bucket, args.baseline_file)
        compare(args.test, baseline_data)
        sys.exit(0)

    if args.update:
        baseline_data = load_baseline_file(args.gcs_bucket, args.baseline_file)
        result = add_to_baseline_file(args.update, baseline_data,
                args.baseline_threshold)
        save_baseline_file(args.gcs_bucket, args.baseline_file, result)
        sys.exit(0)
