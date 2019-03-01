#!/usr/bin/env bash

set -e
REGION=us-central1

zones=$(gcloud compute zones list --filter="region=$REGION" | grep UP | cut -f1 -d' ')
success=false
for zone in $zones; do
    echo "Trying zone $zone"
    gcloud config set compute/zone "${zone}"
    if gcloud compute instances create \
        --image-family ubuntu-1804-lts \
        --image-project ubuntu-os-cloud \
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

sleep 30  # give it time to boot
gitdir=$PWD
cd
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
gcloud compute scp collector.tar.gz "collector-nb-${CIRCLE_BUILD_NUM}":
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "pwd && ls -Fh"
exit 0
