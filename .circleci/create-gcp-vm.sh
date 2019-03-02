#!/usr/bin/env bash

IMAGE_COLLECTOR="$1"
shift
IMAGE_KERNEL_MODULES="$1"
shift
DOCKER_USER="$1"
shift
DOCKER_PASS="$1"
shift

set -e
REGION=us-central1

#zones=$(gcloud compute zones list --filter="region=$REGION" | grep UP | cut -f1 -d' ')
success=false
for zone in us-central1-a us-central1-b ; do
    echo "Trying zone $zone"
    gcloud config set compute/zone "${zone}"
    if gcloud compute instances create \
      --image-family ubuntu-1804-lts \
      --image-project ubuntu-os-cloud \
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
cd /tmp
echo "A001"
gitdir=${SOURCE_ROOT}
cd ..
echo "A005"
rm -rf s2
echo "A007"
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "(which docker || DEBIAN_FRONTEND=noninteractive sudo apt install -y make cmake g++ gcc apt-transport-https ca-certificates curl gnupg-agent wget software-properties-common && curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add - && sudo add-apt-repository 'deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable' && sudo apt update -y && DEBIAN_FRONTEND=noninteractive sudo apt install -y docker-ce && sudo adduser $(id -un) docker)"

gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "docker login -u '$DOCKER_USER' -p '$DOCKER_PASS'"
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "docker pull ${IMAGE_COLLECTOR}"
gcloud compute ssh "collector-nb-${CIRCLE_BUILD_NUM}" --command "docker pull ${IMAGE_KERNEL_MODULES}"
echo "A008"
exit 0
