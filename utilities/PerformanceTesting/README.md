# Prerequisites

Install the following

kubectl
infra
helm
Rox Workflow scripts

If you want to run with an applied load you will also need kubenetbench

# Purpose

Spins up an openshift-4 cluster and repeatedly runs StackRox with different versions of Collector and
reports metrics such as cpu and memory usage for each run.

# Running

The first time you will want to run the following command

```
helm repo add rhacs https://mirror.openshift.com/pub/rhacs/charts
````

To run StackRox on an openshift-4 cluster execute the following


```
./performance-test.sh <cluster_name> <test_dir> <load-test-name> <num-streams> <collector_versions_file> [teardown_script] [nrepeat] [sleep_after_stack_rox] [load_duration] [query_window] [artifacts_dir]
```

Setting the environment variable TEARDOWN script is recommended. An alternative is to pass is to pass it as a parameter.

- `cluster_name`: Name of the openshift-4 cluster

- `test_dir`: The test results will be written to `test_dir` directory. The path of the result files will be
	`test_dir/results_<nick_name>_<run>.txt`. Where `<nick_name>` is from the third column of the
	<collector_version_file>. More on that below.

- `load-test-name`: If network load is applied this will be the name for the Kubnetbench load test. The Kubenetbench will create a directory with
	results regarding the network load and other files.

- `collector_version_file`: A file containing a list of collector images and associated nick names
	To be more exact the format is: `collector_image_registry collector_image_tag nick_name`

	An example can be found in `collector_versions.txt`. The point of `nick_name` is to make it more easily
	identifiable what a Collector image is.

- `teardown_script`: StackRox needs to be torndown between runs and that requires the teardown script
	in the Workflow repo. This is an optional parameter, but if it is not provided on the command line
	the `TEARDOWN_SCRIPT` environment variable must be set

- `nrepeat`: How many times are the tests repeated. The default is `5`

- `sleep_after_start_stack_rox`: The time in seconds between when StackRox is started and when the network load is applied. The default is `60`.

- `load_duration`: The duration of the network load in seconds. The default is `600`.

- `query_window`: The time window used by the Prometheus queries. The default is `10m`.

- `artifacts_dir`: Where information about the cluster is stored. This is needed by almost all scripts in
	this directory. If you are running multiple clusters at the same time this needs to be different
	for each cluster. The default is `/tmp/artifacts`

In addition the following environment variables may be set:

- Configure central image being used:
  - `CENTRAL_IMAGE_REGISTRY`
  - `CENTRAL_IMAGE_NAME`
  - `CENTRAL_IMAGE_TAG`

- Configure scanner DB image being used:
  - `SCANNER_DBIMAGE_REGISTRY`
  - `SCANNER_DBIMAGE_NAME`
  - `SCANNER_DBIMAGE_TAG`

- Configure scanner image being used:
  - `IMAGE_MAIN_REGISTRY`
  - `IMAGE_MAIN_NAME`
  - `IMAGE_MAIN_TAG`

The purpose of these environment variables is to control the versions of the other componenets of StackRox.

# Output

The test results will be written to `test_dir` directory. The path of the result files will be
`test_dir/results_<nick_name>_<run>.txt`. Where `<nick_name>` is from the third column of the
`<collector_version_file>`. The results are the output of the query.sh script which is called
from the `performance-test.sh` script. 

The output format is in json.


Averaged results over multiple iterations will be saved to `<test_dir>/Average_results_<nick_name>.json`
for each collector version.


# Getting Averages Over Iterations

To get averaged runs from multiple iterations run

```
	python3 GetAverages.py --filePrefix <file_prefix> --numFiles <num_files> --outputFile <output_file>
```

When a new metric is added there is no need to change GetAverages.py

### Example

Say the following files were outputted by the query.sh script as a result of running performance-test.sh


TestResults/results_0.json
TestResults/results_1.json
TestResults/results_2.json
TestResults/results_3.json
TestResults/results_4.json
TestResults/results_5.json

To average theses runs something like the following could be executed

```
       python3 GetAverages.py --filePrefix TestResults/results_ --numFiles 6 --outputFile TestResults/Average_results.txt
```
