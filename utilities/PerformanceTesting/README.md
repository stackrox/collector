# Prerequisites

Install the following

kubectl
infra
helm

# Running

The first time you will want to run the following command

```
helm repo add rhacs https://mirror.openshift.com/pub/rhacs/charts
````

To run Rox on an openshift-4 cluster execute the following

```
./PerformanceTest.sh <cluster_name> [artifacts_dir]
```

To run the queries once you have a cluster up and running execute the following


```
./queries.sh
```
