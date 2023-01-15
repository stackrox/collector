#!/usr/bin/env bash
set -eou pipefail

vm_name=$1
test_dir=../test/vm-test

image_family=ubuntu-2204-lts
image_project=ubuntu-os-cloud

vm_command() {
    cmd=$1

    gcloud compute ssh "$vm_name" --project stackrox-dev --command "$cmd" || true
}


gcloud compute instances create --image-family "$image_family" --image-project "$image_project" --project stackrox-dev --machine-type e2-standard-2 --boot-disk-size=20GB "$vm_name"
gcloud compute scp "$test_dir" "$vm_name":~/test_dir --recurse --project stackrox-dev || true
vm_command "git clone https://github.com/stackrox/collector.git"
vm_command "git clone https://github.com/stackrox/workflow.git"
vm_command "cd collector; git checkout jv-rox-13324-performance-testing"
vm_command "sudo snap install kubectl --classic"
vm_command "sudo snap install jq"
vm_command "sudo snap install helm --classic"
vm_command "sudo apt-get install unzip -y"
vm_command "sudo apt-get install make -y"
vm_command "sudo apt-get install golang -y"
vm_command 'curl --fail -sL https://infra.rox.systems/v1/cli/linux/amd64/upgrade | jq -r ".result.fileChunk" | base64 -d > $HOME/infractl'
vm_command "chmod 755 ~/infractl"
vm_command "sudo mv ~/infractl /usr/local/bin"
vm_command "wget https://github.com/openshift/origin/releases/download/v3.11.0/openshift-origin-client-tools-v3.11.0-0cbc58b-linux-64bit.tar.gz"
vm_command "tar xzf openshift-origin-client-tools-v3.11.0-0cbc58b-linux-64bit.tar.gz"
vm_command "sudo cp ~/openshift-origin-client-tools-v3.11.0-0cbc58b-linux-64bit/oc /usr/local/bin"
vm_command "curl -O https://mirror.openshift.com/pub/rhacs/assets/latest/bin/Linux/roxctl"
vm_command "chmod 755 ~/roxctl"
vm_command "sudo cp ~/roxctl /usr/local/bin"
vm_command 'cd ~/test_dir; cur_dir="$(pwd)"; sed -i "s|VM_TEST_DIR|$cur_dir|" config.json'
vm_command "helm repo add rhacs https://mirror.openshift.com/pub/rhacs/charts"
vm_command "export DOCKER_USERNAME=$DOCKER_USERNAME; export DOCKER_PASSWORD=$DOCKER_PASSWORD; export INFRA_TOKEN=$INFRA_TOKEN; export TEARDOWN_SCRIPT=workflow/scripts/runtime/teardown.sh; $HOME/collector/utilities/PerformanceTesting/loop-through-num-ports.sh $HOME/test_dir/config.json"
