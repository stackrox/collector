#!/usr/bin/env bash

DOCKER_USER="$1"
shift
DOCKER_PASS="$1"
shift
SOURCE_ROOT="$1"
shift

set -e
REGION=us-central1

#zones=$(gcloud compute zones list --filter="region=$REGION" | grep UP | cut -f1 -d' ')
success=false
for zone in us-central1-a us-central1-b ; do
    echo "Trying zone $zone"
    gcloud config set compute/zone "${zone}"
    if gcloud compute instances create \
      --image-family cos-stable \
      --image-project cos-cloud \
      --service-account=circleci-collector@stackrox-ci.iam.gserviceaccount.com \
        "collector-nb-${CIRCLE_BUILD_NUM}"
    then
				success=true
        break
    else
        gcloud compute instances delete "collector-nb-${CIRCLE_BUILD_NUM}"
    fi
done

if test ! "$success" = "true" ; then
  echo "Could not boot instance."
  exit 1
fi

echo "A000"
sleep 30  # give it time to boot
gitdir="$SOURCE_ROOT"
cd /tmp
git clone $gitdir shipdir
rm -rf shipdir/.git
echo $CIRCLE_BUILD_NUM > shipdir/buildnum.txt
mkdir s2
mv shipdir s2/collector
cd s2
tar cvfz collector.tar.gz collector/
cd ..
mv s2/collector.tar.gz .
rm -rf s2
TOOLBOX=toolbox

gcloud compute scp collector.tar.gz "collector-nb-${CIRCLE_BUILD_NUM}":
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "set -x ; export DEBIAN_FRONTEND=noninteractive ; sudo $TOOLBOX apt update -y && sudo $TOOLBOX apt install -y make cmake g++ gcc apt-transport-https ca-certificates curl gnupg-agent wget software-properties-common ca-certificates procps
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "set -x ; docker login -u '$DOCKER_USER' -p '$DOCKER_PASS'"
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "set -x ; $TOOLBOX tar xvpfz collector.tar.gz && rm collector.tar.gz"
echo "A008"
exit 0
