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

To run Rox on an openshift-4 cluster execute the following

```
./performance-test.sh <cluster_name> <test_dir> <load-test-name> <num-streams> <collector_versions_file> [teardown_script] [nrepeat] [artifacts_dir]
```

cluster_name: Name of the openshift-4 cluster

test_dir: The test results will be written to test_dir directory. The path of the result files will be
	test_dir/result_<nick_name>_<run>.txt. Where <nick_name> is from the third column of the
	<collector_version_file>. More on that below.	 

load-test-name: If network load is applied this will be the name for the Kubnetbench load test. The Kubenetbench will create a directory with
	results regarding the network load and other files.

collector_version_file: A file containing a list of collector images and associated nick names
	To be more exact the format is

```
	collector_image_registry collector_image_tag nick_name
```

An example can be found in collector_versions.txt. The point of nick_name is to make it more easily
identifiable what a Collector image is.

teardown_script: StackRox needs to be torndown between runs and that requires the teardown script
in the Workflow repo. This is an optional parameter, but if it is not provided on the command line
the TEARDOWN_SCRIPT environment variable must be set

nrepeat: How many times are the tests repeated. The default is 5

artifacts_dir: Where information about the cluster is stored. This is needed by almost all scripts in
	this directory. If you are running multiple clusters at the same time this needs to be different
	for each cluster. The default is /tmp/artifacts

In addition the following environment variables may be set:

CENTRAL_IMAGE_REGISTRY
CENTRAL_IMAGE_NAME
CENTRAL_IMAGE_TAG

SCANNER_DBIMAGE_REGISTRY
SCANNER_DBIMAGE_NAME
SCANNER_DBIMAGE_TAG

IMAGE_MAIN_REGISTRY
IMAGE_MAIN_NAME
IMAGE_MAIN_TAG

The purpose of these environment variables is to control the versions of the other componenets of StackRox.
