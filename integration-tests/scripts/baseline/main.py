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
the test file (with/without collector). Then it will perform an IQR test to
find out if the test data is 1.5-outlier.

NOTE: Originally it was doing t-test for mean values, but it seems to be too
sensitive for such variance.
"""

import argparse
import json
import os
import sys

from itertools import groupby
from functools import partial
from datetime import (datetime, timedelta)
from scipy import stats

from google.oauth2 import service_account
from google.cloud import storage
from google.api_core.exceptions import NotFound


DEFAULT_GCS_BUCKET = "stackrox-ci-artifacts"
DEFAULT_BASELINE_PATH = ""
DEFAULT_BASELINE_FILE = "collector/baseline/all.json"
DEFAULT_BASELINE_THRESHOLD = 10

# For the sake of simplicity Baseline data stored on GCS is simply an array of
# benchmark runs in json format. It maybe beneficial though to change it in the
# future, depending on the incoming requirements. Encode the current structure
# in the empty document to be explicit.
EMPTY_BASELINE_STRUCTURE = []

# In which format we expect to find dates in the baseline files
DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def object_parser_hook(data):
    result = data

    for f in (datetime_parser, memory_parser):
        result = f(result)

    return result


def memory_parser(data):
    result = {}
    multipliers = {
        "GiB": 1024,
        "MiB": 1,
        "KiB": 1 / 1024,
    }

    for k, v in data.items():
        if k in ('Mem'):
            suffix = v[-3:]
            multiplier = multipliers.get(suffix, 0)

            if multiplier == 0:
                result[k] = 0
            else:
                result[k] = float(v[:-3]) * multiplier
        else:
            result[k] = v

    return result


def datetime_parser(data):
    result = {}

    for k, v in data.items():
        if k in ('Timestamp', 'LoadStartTs', 'LoadStopTs'):
            try:
                result[k] = datetime.strptime(v, DATE_FORMAT)
            except Exception as ex:
                print(f"Could not parse timestamp {v}: {ex}")
                result[k] = v
        else:
            result[k] = v

    return result


def get_gcs_blob(bucket_name, filename):
    credentials = json.loads(os.environ["GOOGLE_CREDENTIALS_COLLECTOR_CI_VM_SVC_ACCT"])
    storage_credentials = service_account.Credentials.from_service_account_info(credentials)
    storage_client = storage.Client(credentials=storage_credentials)

    bucket = storage_client.bucket(bucket_name)
    return bucket.blob(filename)


def load_baseline_from_file(file_path, baseline_file):
    with open(f"{file_path}/{baseline_file}", "r") as baseline:
        return json.loads(baseline.read(), object_hook=object_parser_hook)


def load_baseline_from_bucket(bucket_name, baseline_file):
    blob = get_gcs_blob(bucket_name, baseline_file)

    try:
        contents = blob.download_as_string()
        return json.loads(contents, object_hook=object_parser_hook)

    except NotFound:
        print(f"File gs://{bucket_name}/{baseline_file} not found. "
              f"Creating a new empty one.", file=sys.stderr)

        blob.upload_from_string(json.dumps(EMPTY_BASELINE_STRUCTURE))
        return []


def save_baseline_to_file(file_path, baseline_file, data):
    with open(f"{file_path}/{baseline_file}", "w") as baseline:
        baseline.write(json.dumps(data, default=str))
        baseline.truncate()


def save_baseline_to_bucket(bucket_name, baseline_file, data):
    blob = get_gcs_blob(bucket_name, baseline_file)
    blob.upload_from_string(json.dumps(data, default=str))


def is_test(test_name, record):
    return record.get("TestName") == test_name


def get_timestamp(record):
    return record.get("Timestamp")


def add_to_baseline_file(input_file_name, baseline_data, threshold):
    if baseline_data:
        verify_data(baseline_data)

    with open(input_file_name, "r") as measure:
        new_measurement = json.load(measure, object_hook=object_parser_hook)
        verify_data(new_measurement)

        new_measurement_keys = [
            data[0] for data in group_data(new_measurement,
                                           "VmConfig", "CollectionMethod")
        ]

        # Baseline contains several tests per one benchmark record
        test_names = set(value["TestName"] for value in baseline_data)

        result = []
        for key, values in group_data(baseline_data, "VmConfig", "CollectionMethod"):
            if key not in new_measurement_keys:
                # Keep the original data in baseline, even if there is nothing
                # like that in the new benchmark. No need for any kind of
                # verification.
                result.extend(values)
                continue

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


def verify_data(data):
    """
    There are some assumptions made about the data we operate with for both
    baseline series and new measurements, they're going to be verified below.
    """

    assert data, "Benchmark data must be not empty"


def intersection(baseline_data, new_data):
    """
    Remove any tests that are not found in both the baseline and the new data.

    This is done for 2 reasons:
    - Data that shows up in the baseline but not in the new data is considered a
      removed test. There is no point in processing this.
    - Data that shows up in the new data but not in the baseline is considered
      a new test that doesn't have an associated baseline yet. The baseline for
      the new test will be added after it is merged into master.
    """
    assert baseline_data, "Baseline data must be not empty"

    baseline_grouped = process(baseline_data)
    new_grouped = process(new_data)

    new_keys = [items[0] for items in new_grouped]
    baseline_filtered = [
        data for data in baseline_grouped
        if data[0] in new_keys
    ]

    baseline_keys = [items[0] for items in baseline_filtered]
    new_filtered = [
        data for data in new_grouped
        if data[0] in baseline_keys
    ]

    return (baseline_filtered, new_filtered)


def normalize_collection_method(method):
    if any(m in method for m in ('kernel', 'module')):
        return 'module'
    if 'ebpf' in method:
        return 'ebpf'
    if 'core' in method:
        return 'core-bpf'
    raise Exception(f'Invalid collection method: {method}')


def process(content):
    """
    Transform benchmark data into the format CI scripts work with, and group by
    VmConfig and CollectionMethod fields.
    """

    processed = [
        {
            "kernel": record.get("VmConfig").replace('_', '.'),
            "collection_method": normalize_collection_method(record.get("CollectionMethod")),
            "timestamp": stats.get("Timestamp"),
            "test": record.get("TestName"),
            "cpu": stats.get("Cpu"),
            "mem": stats.get("Mem")
        }
        for record in content
        for stats in record.get("ContainerStats")

        # Filter statistics from Collector only, bounded to the timeframe when
        # load was actually running. LoadStopTs is adjusted by 1 sec, since
        # this timestamp is taken after the workload container was stopped, so
        # that the actual load might have stopped slightly earlier.
        if (stats.get("Name") == "collector" and
            stats.get("Timestamp") > record.get("LoadStartTs") and
            stats.get("Timestamp") < record.get("LoadStopTs") - timedelta(seconds=1))
    ]

    return group_data(processed, "kernel", "collection_method")


def group_data(content, *columns):
    def record_id(record):
        return " ".join([record.get(c) for c in columns])

    ordered = sorted(content, key=record_id)
    return [
        (key, list(group))
        for key, group in groupby(ordered, key=record_id)
    ]


def split_benchmark(measurements):
    no_overhead = []
    overhead = []

    # This has to cover versions of the tests that tag perf data
    # with "*_benchmark" or automatically with the test name.
    for m in measurements:
        if "baseline_benchmark" in m:
            no_overhead.append(m["baseline_benchmark"])

        if "TestBenchmarkBaseline" in m:
            no_overhead.append(m["TestBenchmarkBaseline"])

        if "collector_benchmark" in m:
            overhead.append(m["collector_benchmark"])

        if "TestBenchmarkCollector" in m:
            overhead.append(m["TestBenchmarkCollector"])

    return (no_overhead, overhead)


def collector_overhead(measurements):
    """
    Express collector overhead in absolute difference in hackbench_avg_time.
    Due to variance a relative difference (i.e. overhead / no overhead) will
    also have higher variance, because the actual delta will be smaller/larger
    relative to the total timing.
    """
    no_overhead, overhead = split_benchmark(measurements)

    return [x - y for (x, y) in zip(overhead, no_overhead)]


def compare(input_file_name, baseline_data):
    if not baseline_data:
        print("Baseline file is empty, nothing to compare.", file=sys.stderr)
        return

    verify_data(baseline_data)

    with open(input_file_name, "r") as measurement:
        test_data = json.load(measurement, object_hook=object_parser_hook)
        verify_data(test_data)

        baseline_grouped, test_grouped = intersection(baseline_data, test_data)

        for (baseline, test) in zip(baseline_grouped, test_grouped):
            bgroup, bvalues = baseline
            tgroup, tvalues = test

            assert bgroup == tgroup, "Kernel/Method must not be differrent"

            baseline_cpu = [v.get("cpu") for v in bvalues]
            baseline_mem = [v.get("mem") for v in bvalues]

            test_cpu = [v.get("cpu") for v in tvalues]
            test_mem = [v.get("mem") for v in tvalues]

            # Originally we were doing one-sample test for
            # the the new data to be a 1.5 outlier
            #
            # iqr = stats.iqr(baseline_cpu)
            # q1, q3 = np.percentile(baseline_cpu, [25, 75])
            # lower = q1 - 1.5 * iqr
            # upper = q3 + 1.5 * iqr
            # outlier = 0 if lower < test_cpu < upper else 1
            cpu_stats, cpu_pvalue = stats.ttest_ind(baseline_cpu, test_cpu)
            mem_stats, mem_pvalue = stats.ttest_ind(baseline_mem, test_mem)

            baseline_cpu_median = round(stats.tmean(baseline_cpu), 2)
            baseline_mem_median = round(stats.tmean(baseline_mem), 2)

            test_cpu_median = round(stats.tmean(test_cpu), 2)
            test_mem_median = round(stats.tmean(test_mem), 2)

            print(f"{bgroup} {baseline_cpu_median} {test_cpu_median} {cpu_pvalue} "
                  f"{baseline_mem_median} {test_mem_median} {mem_pvalue}")


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

    parser.add_argument('--baseline-path', nargs='?', default=DEFAULT_BASELINE_PATH,
                        help=('If specified, overrides --gcs-bucket and instructs'
                              ' to manage baselines by specified file path'))

    args = parser.parse_args()

    if args.baseline_path:
        baseline_data = load_baseline_from_file(args.baseline_path, args.baseline_file)

        save_baseline_file = partial(save_baseline_to_file, args.baseline_path, args.baseline_file)
    else:
        baseline_data = load_baseline_from_bucket(args.gcs_bucket, args.baseline_file)

        save_baseline_file = partial(save_baseline_to_bucket, args.gcs_bucket, args.baseline_file)

    if args.test:
        compare(args.test, baseline_data)
        sys.exit(0)

    if args.update:
        result = add_to_baseline_file(args.update, baseline_data,
                                      args.baseline_threshold)
        save_baseline_file(result)
        sys.exit(0)
