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
import time

from itertools import groupby, zip_longest
from collections import Counter
from scipy import stats

from google.oauth2 import service_account
from google.cloud import storage
from google.api_core.exceptions import NotFound


DEFAULT_GCS_BUCKET = "stackrox-ci-results"
DEFAULT_BASELINE_FILE = "circleci/collector/baseline/all.json"
DEFAULT_BASELINE_THRESHOLD = 10

# For the sake of simplicity Baseline data stored on GCS is simply an array of
# benchmark runs in json format. It maybe beneficial though to change it in the
# future, depending on the incoming requirements. Encode the current structure
# in the empty document to be explicit.
EMPTY_BASELINE_STRUCTURE = []


def get_gcs_blob(bucket_name, filename):
    credentials = json.loads(os.environ["GOOGLE_CREDENTIALS_CIRCLECI_COLLECTOR"])
    storage_credentials = service_account.Credentials.from_service_account_info(credentials)
    storage_client = storage.Client(credentials=storage_credentials)

    bucket = storage_client.bucket(bucket_name)
    return bucket.blob(filename)


def load_baseline_file(bucket_name, baseline_file):
    blob = get_gcs_blob(bucket_name, baseline_file)

    try:
        contents = blob.download_as_string()
        return json.loads(contents)

    except NotFound:
        print(f"File gs://{bucket_name}/{baseline_file} not found. "
              f"Creating a new empty one.", file=sys.stderr)

        blob.upload_from_string(json.dumps(EMPTY_BASELINE_STRUCTURE))
        return []


def save_baseline_file(bucket_name, baseline_file, data):
    blob = get_gcs_blob(bucket_name, baseline_file)
    blob.upload_from_string(json.dumps(data))


def is_test(test_name, record):
    return record.get("TestName") == test_name


def get_timestamp(record):
    field = record.get("Timestamp")
    parsed = time.strptime(field, "%Y-%m-%d %H:%M:%S")
    return time.mktime(parsed)


def add_to_baseline_file(input_file_name, data, threshold):
    with open(input_file_name, "r") as measure:
        new_measurement = json.load(measure)

        if data:
            verify_new_data(data, new_measurement)

        # Baseline contains several tests per one benchmark record
        test_names = set(value["TestName"] for value in data)

        result = []
        for _, values in group_data(data, "VmConfig", "CollectionMethod"):
            # For every group (vm, method) sort the records in ascending order
            # to get rid of the oldest data (at the beginning or the list).
            ordered = sorted(values, key=get_timestamp)

            # Verify for each test type whithin the data, that its length is
            # not over the threshold. Drop the oldest items when needed.
            for t in test_names:
                tests = [item for item in ordered if is_test(t, item)]

                if len(tests) >= threshold:
                    cutoff = len(tests) - threshold
                    result.extend(tests[cutoff:])
                else:
                    result.extend(tests)

        result.extend(new_measurement)
        return result


def verify_new_data(baseline_data, new_data):
    """
    Enforce compatibility between baseline data and a new chunk of numbers. Two
    invariants needs to be maintained:

    * both baseline and new data have the same set of (VmConfig, CollectionMethod)

    * New data provides equal number of with/without collector benchmark runs

    The function expects baseline is not empty.

    """

    assert baseline_data, "Baseline data must be not empty"

    baseline_grouped = process(baseline_data)
    new_grouped = process(new_data)

    for (baseline, new) in zip_longest(baseline_grouped, new_grouped):
        bgroup, bvalues = baseline
        ngroup, nvalues = new

        if bgroup != ngroup:
            raise Exception(f"Benchmark metrics do not match:"
                            f" {bgroup}, {ngroup} ")

        counter = Counter()
        for v in nvalues:
            counter.update(v.keys())

        # We rely only on baseline/collector hackbench at the moment. As soon
        # as data for more tests will be collected, this section has to be
        # extended accordingly.
        no_overhead_count = counter.get("baseline_benchmark", 0)
        overhead_count = counter.get("collector_benchmark", 0)

        if (no_overhead_count != overhead_count):
            raise Exception(f"Number of with/without overhead do not match:"
                            f" {no_overhead_count}, {overhead_count} ")


def process(content):
    """
    Transform benchmark data into the format CI scripts work with.
    """

    processed = [
        {
            "kernel": record.get("VmConfig"),
            "collection_method": record.get("CollectionMethod"),
            "timestamp": record.get("Timestamp"),
            record["TestName"]: record.get("Metrics").get("hackbench_avg_time")
        }
        for record in content
        if record.get("Metrics", {}).get("hackbench_avg_time")
    ]

    return group_data(processed, "kernel", "collection_method")


def group_data(content, *columns):
    def record_id(record):
        return " ".join([record.get(c) for c in columns])

    ordered = sorted(content, key=record_id)
    return groupby(ordered, key=record_id)


def split_benchmark(measurements):
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

    return (no_overhead, overhead)


def collector_overhead(measurements):
    no_overhead, overhead = split_benchmark(measurements)

    return [
        round(100 * x / y, 2)
        for (x, y) in zip(overhead, no_overhead)
    ]


def compare(input_file_name, baseline_data):
    if not baseline_data:
        print("Baseline file is empty, nothing to compare.", file=sys.stderr)
        return

    with open(input_file_name, "r") as measurement:
        test_data = json.load(measurement)
        verify_new_data(baseline_data, test_data)

        baseline_grouped = process(baseline_data)
        test_grouped = process(test_data)

        for (baseline, test) in zip(baseline_grouped, test_grouped):
            bgroup, bvalues = baseline
            tgroup, tvalues = test

            assert bgroup == tgroup, "Kernel/Method must not be differrent"

            bvalues = list(bvalues)
            tvalues = list(tvalues)

            baseline_overhead = collector_overhead(bvalues)
            test_overhead = collector_overhead(tvalues)[0]
            result, pvalue = stats.ttest_1samp(baseline_overhead,
                                               test_overhead)

            benchmark_baseline, benchmark_collector = split_benchmark(bvalues)
            test_baseline, test_collector = split_benchmark(tvalues)

            baseline_median = round(stats.tmean(benchmark_baseline), 2)
            collector_median = round(stats.tmean(benchmark_collector), 2)

            print(f"{bgroup} {test_baseline[0]} {test_collector[0]} "
                  f"{baseline_median} {collector_median} {round(pvalue, 2)}")


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
