# Deployment

## Overview

Ansible playbooks orchestrate VM lifecycle, environment provisioning, and test execution across cloud platforms and operating systems.

**Tool:** Ansible 2.9+
**Platforms:** GCP, IBM Cloud (Power/Z), Local VMs
**OS:** RHEL, CentOS, Ubuntu, SLES, Flatcar, Fedora CoreOS, Container-Optimized OS, Garden Linux
**Runtimes:** Docker, Podman, CRI-O, containerd

## Deployment Flow

```
Ansible Playbook
    ↓
1. create-all-vms (GCP/IBM Cloud API)
   ├── Compute instances
   ├── Networking (VPC, subnets)
   └── SSH keys / Floating IPs
    ↓
2. provision-vm
   ├── Install Python (Flatcar/CoreOS)
   ├── Update packages
   ├── Install Docker/Podman
   ├── Configure SELinux
   └── Pull collector images
    ↓
3. run-test-target
   ├── Login to registry
   ├── Execute tests
   ├── Collect logs
   └── Save results
    ↓
4. destroy-vm (cleanup)
   ├── Delete instances
   └── Release floating IPs
```

## Directory Structure

```
ansible/
├── ci/                      # CI inventory
│   ├── gcp.yml             # GCP dynamic inventory
│   └── ibmcloud.yml        # IBM Cloud dynamic
├── dev/                     # Dev inventory
├── roles/                   # Ansible roles
│   ├── create-vm/
│   ├── create-all-vms/
│   ├── provision-vm/
│   ├── run-test-target/
│   └── destroy-vm/
├── group_vars/
│   ├── all.yml             # VM definitions
│   ├── platform_flatcar.yml
│   └── platform_*.yml
├── integration-tests.yml
├── k8s-integration-tests.yml
├── benchmarks.yml
├── vm-lifecycle.yml
├── ci-build-builder.yml
└── ci-build-collector.yml
```

## Playbooks

### integration-tests.yml

End-to-end: create VMs → provision → test → destroy.

Tags:
- setup: Create VMs
- provision: Provision VMs
- run-tests: Execute tests
- teardown: Destroy VMs

Usage:
```bash
VM_TYPE=rhel ansible-playbook -i dev integration-tests.yml
ansible-playbook -i dev integration-tests.yml --tags provision
ansible-playbook -i dev integration-tests.yml --tags run-tests
```

Variables:
- VM_TYPE: VM family (rhel, ubuntu-os, cos)
- JOB_ID: Unique identifier ($USER)
- COLLECTOR_TEST: Test target (ci-integration-tests)
- GCP_SSH_KEY_FILE: SSH key (~/.ssh/google_compute_engine)

### k8s-integration-tests.yml

Tests on Kubernetes (KinD or existing cluster).

Variables:
- tester_image: Collector test image (required)
- collector_image: Collector image (required)
- collector_root: Repo path (required)
- cluster_name: KinD cluster (collector-tests)
- container_engine: Docker or podman (docker)

Tags:
- test-only: Skip KinD creation/deletion
- cleanup: Clean K8s resources

Usage:
```bash
cat > k8s-vars.yml <<EOF
tester_image: quay.io/rhacs-eng/collector-tests:$(git describe --tags)
collector_image: quay.io/stackrox-io/collector:$(git describe --tags)
collector_root: $(pwd)
EOF

ansible-playbook -i dev/localhost.yml -e '@k8s-vars.yml' k8s-integration-tests.yml
ansible-playbook -i dev/localhost.yml -e '@k8s-vars.yml' --tags test-only k8s-integration-tests.yml
```

### vm-lifecycle.yml

Manage VM lifecycle independently.

```bash
VM_TYPE=ubuntu-os ansible-playbook -i dev vm-lifecycle.yml --tags setup
ansible-playbook -i dev vm-lifecycle.yml --tags provision
ansible-playbook -i dev vm-lifecycle.yml --tags teardown
```

### benchmarks.yml

Performance benchmarks (baseline vs. collector).

```bash
VM_TYPE=rhel ansible-playbook -i dev benchmarks.yml
```

### ci-build-builder.yml

Build/push collector-builder (CI).

Variables:
- arch: amd64, arm64, ppc64le, s390x
- stackrox_io_username/password: Quay.io
- rhacs_eng_username/password: Quay.io

```bash
ansible-playbook ci-build-builder.yml \
    -e arch=amd64 \
    -e stackrox_io_username=$QUAY_USER \
    -e stackrox_io_password=$QUAY_PASS
```

### ci-build-collector.yml

Retag/push collector (CI). Same variables as builder plus collector_image.

## Roles

### create-vm

Create single VM on GCP or IBM Cloud.

GCP:
```yaml
- name: Create GCP instance
  gcp_compute_instance:
    name: "{{ vm_name }}"
    machine_type: "{{ vm_machine_type }}"
    zone: "{{ vm_zone }}"
    project: "{{ gcp_project }}"
    boot_disk:
      initialize_params:
        source_image: "{{ vm_image }}"
    metadata:
      ssh-keys: "{{ ssh_public_key }}"
```

IBM Cloud (Z/Power):
```yaml
- name: Create VPC instance
  ibm.cloudcollection.ibm_is_instance:
    name: "{{ vm_name }}"
    vpc: "{{ ibm_vpc_id }}"
    profile: "{{ vm_profile }}"
    image: "{{ vm_image_id }}"
    keys: ["{{ ibm_ssh_key_id }}"]

- name: Create Floating IP
  ibm.cloudcollection.ibm_is_floating_ip:
    name: "{{ vm_name }}-fip"
    target: "{{ instance.id }}"
```

### provision-vm

Configure OS, install container runtime.

Flow:
1. Wait for SSH
2. Install Python (Flatcar/CoreOS)
3. Gather facts
4. Platform-specific provisioning
5. Install runtime (Docker/Podman)
6. Configure SELinux

Fedora CoreOS:
```yaml
- name: Install Python3
  raw: |
    rpm-ostree install python3 python3-dnf && systemctl reboot
  become: yes

- name: Wait for reboot
  wait_for_connection:
    timeout: 300
```

Flatcar:
```yaml
- name: Create Python symlink
  raw: |
    if [ ! -f /opt/bin/python ]; then
      /usr/bin/toolbox --bind=/opt/bin \
        /bin/sh -c 'ln -s /usr/bin/python3 /opt/bin/python'
    fi
```

RHEL:
```yaml
- name: Register with Red Hat
  redhat_subscription:
    state: present
    username: "{{ redhat_username }}"
    password: "{{ redhat_password }}"

- name: Install Docker
  yum:
    name: [docker, docker-ce]
    state: present
```

### run-test-target

Execute tests on provisioned VM.

```yaml
- name: Login to quay.io
  shell: |
    {{ runtime_command }} login -u {{ quay_username }} \
      --password-stdin quay.io
  stdin: "{{ quay_password }}"

- name: Run tests
  shell: |
    cd {{ collector_repo }}/integration-tests && \
    make {{ collector_test }}
  environment:
    COLLECTION_METHOD: ebpf
    COLLECTOR_IMAGE: "{{ collector_image }}"

- name: Collect logs
  synchronize:
    src: "{{ collector_repo }}/integration-tests/container-logs/"
    dest: "{{ local_log_dir }}/ebpf/"
    mode: pull
```

### destroy-vm

Delete VM and resources.

GCP:
```yaml
- name: Delete instance
  gcp_compute_instance:
    name: "{{ vm_name }}"
    zone: "{{ vm_zone }}"
    state: absent
```

IBM Cloud:
```yaml
- name: Release floating IP
  ibm.cloudcollection.ibm_is_floating_ip:
    id: "{{ floating_ip_id }}"
    state: absent

- name: Delete instance
  ibm.cloudcollection.ibm_is_instance:
    id: "{{ instance_id }}"
    state: absent
```

## Inventory

Dynamic inventory from cloud APIs.

GCP (`ci/gcp.yml`):
```yaml
plugin: gcp_compute
projects: [stackrox-collector-ci]
filters:
  - labels.collector-test = true
keyed_groups:
  - key: labels.job_id
    prefix: job_id
  - key: labels.platform
    prefix: platform
```

IBM Cloud (`ci/ibmcloud.yml`):
```yaml
plugin: ibm.cloudcollection.ibm_vpc
regions: [us-east]
filters:
  - tags contains collector-test
compose:
  ansible_host: network_interfaces[0].floating_ips[0].address
```

Groups: job_id_<id>, platform_<platform>.

Usage:
```yaml
hosts: "job_id_{{ lookup('env', 'JOB_ID') }}"
hosts: platform_rhel
hosts: "job_id_{{ job_id }}:&platform_rhel"
```

## VM Definitions

Location: `group_vars/all.yml`

Structure:
```yaml
vm_definitions:
  <vm_type>:
    cloud: gcp | ibmcloud
    project: <gcp-project> | <ibm-resource-group>
    families:
      - name: <family-name>
        image_family: <image-family>
        machine_type: <machine-type>
        zone: <zone>
        runtime: docker | podman | crio
```

RHEL (GCP):
```yaml
rhel:
  cloud: gcp
  project: stackrox-collector-ci
  families:
    - name: rhel-7
      image_family: rhel-7
      image_project: rhel-cloud
      machine_type: n1-standard-2
      zone: us-central1-a
      runtime: docker
    - name: rhel-8
      image_family: rhel-8
      machine_type: n1-standard-2
      runtime: docker
```

s390x (IBM Cloud):
```yaml
rhel-s390x:
  cloud: ibmcloud
  region: us-east
  families:
    - name: rhel-8-6-s390x
      profile: bz2-1x4
      image_id: r014-12345678-abcd-1234
      vpc_id: r014-vpc-id
      zone: us-east-1
      runtime: podman
```

Naming: `<prefix>-<family>-<job-id>` (e.g., collector-dev-rhel-8-jdoe).

## Container Configuration

Runtime selection priority:
1. VM family: vm_family.runtime
2. VM type default: vm_type.default_runtime
3. Platform group: runtime_command in platform_*.yml
4. Global: docker

Platform defaults:
- cos: docker (pre-installed)
- flatcar: docker (native)
- fedora-coreos: podman (systemd)
- rhel-8+: podman (default)
- rhel-7: docker (compatibility)
- ubuntu/sles/garden: docker

Privileges:
```yaml
docker_run_args:
  - --privileged
  - --pid=host
  - --network=host
  - -v /var/run/docker.sock:/var/run/docker.sock
  - -v /host:/host:ro
  - -v /sys/kernel/debug:/sys/kernel/debug
```

Required for: host PID (process visibility), host network (monitoring), kernel debug FS (eBPF maps), Docker socket (introspection), host filesystem (module loading).

## Kubernetes Deployment

DaemonSet:
```yaml
apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: collector
  namespace: stackrox
spec:
  selector:
    matchLabels:
      app: collector
  template:
    spec:
      hostPID: true
      hostNetwork: true
      containers:
      - name: collector
        image: quay.io/stackrox-io/collector:latest
        env:
        - name: COLLECTION_METHOD
          value: "EBPF"
        - name: GRPC_SERVER
          value: "sensor.stackrox.svc:443"
        resources:
          limits: {cpu: "2", memory: "2Gi"}
          requests: {cpu: "50m", memory: "320Mi"}
        securityContext:
          privileged: true
        volumeMounts:
        - {name: host-root, mountPath: /host, readOnly: true}
        - {name: sys, mountPath: /sys, readOnly: true}
        - {name: certs, mountPath: /var/run/secrets/stackrox.io/certs/}
      volumes:
      - {name: host-root, hostPath: {path: /}}
      - {name: sys, hostPath: {path: /sys}}
      - {name: certs, secret: {secretName: collector-tls}}
```

ConfigMap:
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: runtime-config
data:
  runtime_config.yaml: |
    networking:
      connectionStats:
        aggregationWindow: 15s
      afterglow:
        period: 30s
    processesListening:
      enable: true
    scrape:
      interval: 15s
```

Mount:
```yaml
volumeMounts:
- {name: runtime-config, mountPath: /etc/collector/}
volumes:
- {name: runtime-config, configMap: {name: runtime-config}}
```

## CI/CD

CircleCI (`.circleci/config.yml`):
```yaml
workflows:
  test:
    jobs:
      - build-builder:
          matrix:
            parameters:
              arch: [amd64, arm64, ppc64le, s390x]
      - integration-tests:
          requires: [build-collector]
          matrix:
            parameters:
              vm_type: [rhel, ubuntu-os, cos, flatcar]
```

Job:
```yaml
integration-tests-rhel-ebpf:
  docker:
    - image: quay.io/ansible/ansible-runner:latest
  environment:
    VM_TYPE: rhel
    COLLECTION_METHOD: ebpf
  steps:
    - checkout
    - run:
        name: Setup GCP
        command: echo $GCP_KEY | base64 -d > /tmp/gcp-key.json
    - run: cd ansible && ansible-playbook -i ci integration-tests.yml
```

GitHub Actions (IBM Z/Power):
```yaml
name: Integration Tests (ppc64le)
on:
  push:
    branches: [master, release-*]
jobs:
  test-ppc64le:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: IBM Cloud CLI
        run: |
          curl -fsSL https://clis.cloud.ibm.com/install/linux | sh
          ibmcloud login --apikey ${{ secrets.IC_API_KEY }}
      - name: Run tests
        run: cd ansible && ansible-playbook -i ci integration-tests.yml
        env:
          VM_TYPE: rhel-ppc64le
```

## Security

Quay.io (development):
```bash
cat > ansible/secrets.yml <<EOF
quay_username: "myuser+robot"
quay_password: "ABC123..."
EOF
ansible-vault encrypt ansible/secrets.yml
```

CI: Environment variables QUAY_RHACS_ENG_RO_USERNAME, QUAY_RHACS_ENG_RO_PASSWORD.

Cloud credentials:
```bash
# GCP
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json
# or
gcloud auth application-default login

# IBM Cloud
export IC_API_KEY=<api-key>
export IC_REGION=us-east

# RHEL Subscription
export REDHAT_USERNAME=<username>
export REDHAT_PASSWORD=<password>
```

## Troubleshooting

Ansible connection:
```bash
export ANSIBLE_TIMEOUT=60  # SSH timeout
ansible-playbook -e ansible_python_interpreter=/usr/bin/python3
```

VM creation:
```bash
gcloud compute project-info describe --project=<project>  # GCP quota
ibmcloud is floating-ips --resource-group collector-tests  # IBM Cloud IPs
ansible-playbook -i ci vm-lifecycle.yml --tags teardown    # Cleanup
```

Test execution:
```bash
docker login quay.io -u <username>  # Manual login
docker pull quay.io/stackrox-io/collector:$(git describe --tags)
ansible-playbook -i dev integration-tests.yml --tags run-tests -vvv
```

KinD:
```bash
systemctl status docker     # Check runtime
kind version                # Verify installed
kind create cluster --name collector-tests
kind load docker-image quay.io/stackrox-io/collector:latest --name collector-tests
```

## Utilities

**hotreload.sh** (`utilities/hotreload.sh`): Live reload collector binary in running container.
```bash
./utilities/hotreload.sh /path/to/new/collector-binary
```

**release.py** (`utilities/release.py`): Create release branches/tags, version bumping.
```bash
./utilities/release.py 4.6 --push
```

**tag-bumper.py**, **driver-checksum.sh**, **gardenlinux-bumper/**: Automated updates.

## Key Files

| File | Purpose |
|------|---------|
| ansible/integration-tests.yml | Main test playbook |
| ansible/k8s-integration-tests.yml | K8s tests |
| ansible/vm-lifecycle.yml | VM management |
| ansible/group_vars/all.yml | VM definitions |
| ansible/roles/*/tasks/main.yml | Role logic |
| ansible/ci/gcp.yml | CI inventory |
| utilities/hotreload.sh | Dev helper |

## References

- [Ansible Documentation](https://docs.ansible.com/)
- [GCP Compute Module](https://docs.ansible.com/ansible/latest/collections/google/cloud/)
- [IBM Cloud Collection](https://galaxy.ansible.com/ibm/cloudcollection)
- [Kubernetes DaemonSet](https://kubernetes.io/docs/concepts/workloads/controllers/daemonset/)
- [Integration Tests](integration-tests.md)
- [Build System](build.md)
