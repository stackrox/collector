# Collector Ansible Tools

The files contained within this directory are [ansible](https://www.ansible.com/resources/get-started)
roles and playbooks written to simplify VM life-cycle control, and execution of testing, both
during development and on CI systems[^1].

## Prerequisites

In order to run these playbooks you must have a python version >=3.9.
To install ansible and the necessary dependencies, simply run:

```
$ pip3 install -r requirements.txt
```

On a Mac, the default python3 is 3.7, so use brew to install the latest ansible:

```
$ brew install ansible
$ pip3 install -r requirements.txt
```

## Image builds
### Builder and slim collector images

The `ci-build-collector.yml` playbook is meant to be used by CI, it handles the
build process for the builder and slim collector images, as well as retagging
and pushing of these images to quay.io.

#### Environment variables used by the playbook

| Name | Description |
| ---  | ---         |
| BUILD_BUILDER_IMAGE | Controls whether the builder image should be built or pulled from a remote registry. |
| COLLECTOR_BUILDER_TAG | The tag used for the builder image. |
| PLATFORM | The platform the images are being built for, currently the supported values are:<br>- linux/amd64<br>- linux/ppc64le<br>- linux/s390x |
| COLLECTOR_TAG | The tag to be used with the collector image |
| RHACS_ENG_IMAGE | Similar to the collector_image ansible variable, but for the quay.io/rhacs-eng registry |

#### Ansible variables to be supplied to the playbook

| Name | Description |
| ---  | ---         |
| collector_image | The collector image name to be built, including its tag |
| arch | The architecture the images are being built for, currently the supported values are:<br>- amd64<br>- ppc64le<br>- s390x |
| stackrox_io_username | Username used for pushing images to quay.io/stackrox-io |
| stackrox_io_password | Password used for pushing images to quay.io/stackrox-io |
| rhacs_eng_username | Username used for pushing images to quay.io/rhacs-eng |
| rhacs_eng_password | Password used for pushing images to quay.io/rhacs-eng |
| push_builder | If 'true', the builder will be pushed to the registries alongside the collector images |

## Integration tests
### Overview

The top-level yaml files define playbooks to perform various actions, and can
be run either directly with `ansible-playbook` or through the Makefile targets.

For example, to create and provision RHEL VMs for testing:

```bash
$ VM_TYPE=rhel ansible-playbook -i dev vm-lifecycle.yml --tags setup

# note you only need to specify VM_TYPE when creating VMs
$ ansible-playbook -i dev vm-lifecycle.yml --tags provision
```

Alternatively you can use the `integration-tests.yml` playbook to handle
creation and deletion of VMs as well as running the integration tests against
them:

```bash
$ VM_TYPE=rhel ansible-playbook -i dev integration-tests.yml
```

Note: the `integration-tests.yml` playbook also supports the `setup`, `provision`
`run-tests` and `teardown` tags to run each stage of the process.


### VMs

Within `vars/all.yml` there exists configuration for a variety of VM types,
summarized below:

| Type          | Families       |
| ------------- | -------------- |
| rhel          | rhel-7 <br> rhel-8 |
| rhel-s390x    | rhel-8-6-s390x | 
| rhel-sap      | rhel-8-4-sap-ha <br> rhel-8-6-sap-ha |
| cos           | cos-stable <br> cos-beta <br> cos-dev |
| sles          | sles-12 <br> sles-15 |
| ubuntu-os     | ubuntu-1804-lts <br> ubuntu-2004-lts <br> ubuntu-2204-lts |
| ubuntu-os-pro | ubuntu-pro-1804-lts |
| fedora-coreos | fedora-coreos-stable |
| flatcar       | flatcar-stable |
| garden-linux  | n/a (built from a specific image, see `group_vars/all.yml` for details) |

By specifying `VM_TYPE=<type>` the playbooks will create a VM for every family
listed above. The name of the VMs is defined by the inventory-specific prefix,
the VM family, and the unique ID (which defaults to the current username).

```
<inventory prefix>-<family>-<id>

# e.g.

# A rhel-7 dev VM
collector-dev-rhel-7-12345

# A flatcar-stable CI VM
collector-ci-flatcar-stable-12345
```

### Inventory

The inventory is [dynamic](https://docs.ansible.com/ansible/latest/user_guide/intro_dynamic_inventory.html)
which means that the hosts and groups are all derived from GCP. The `ci/` and `dev/`
subdirectories are [inventory directories](https://docs.ansible.com/ansible/latest/user_guide/intro_dynamic_inventory.html#using-inventory-directories-and-multiple-inventory-sources)
and search the relevant GCP project for VMs that match the unique identifier. They are then grouped
based on that identifier as well as their platform.

The two groups are `job_id_<unique id>` and `platform_<vm type>`

These groups are used to run against only the VMs that have been created by the
same user or CI job.

### Environment Variables

The following environment variables may be used to modify some behavior:

| Variable | Description | Default |
| -------- | ----------- | ------- |
| GCP_SSH_KEY_FILE | The location of the private key file to use with GCP | ~/.ssh/google_compute_engine |
| JOB_ID | A unique identifier to de-conflict VM names | the current user's username |
| COLLECTOR_TEST | Which integration test make target to run. (e.g. integration-test-process-network) | ci-integration-tests |
| VM_TYPE | Which kind of VMs to create on GCP (as listed above.) By default, will only build rhel VMs. Use 'all' to build an inventory containing every kind of VM. | rhel |
| IC_API_KEY | API key for IBM Cloud. See [doc](https://cloud.ibm.com/docs/account?topic=account-userapikey&interface=ui) |
| IC_REGION | The IBM Cloud region where you want to create your resources. See [doc](https://cloud.ibm.com/docs/workload-protection?topic=workload-protection-regions)|
|||

Note: other environment variables that may affect the operation of the integration tests
can be used to modify behavior. See [the integration tests README](../integration-tests/README.md) for details.

### Secrets & Creds

The make targets expect some secrets to exist within a secrets.yml file in the
root of this directory structure. It should contain key/value pairs of variable
names and credentials to be used in the playbooks. Currently, the only required
credentials are quay_username and quay_password, which are created by make
from the environment variables `QUAY_RHACS_ENG_RO_USERNAME` and `QUAY_RHACS_ENG_RO_PASSWORD`
to match CI variables. If you are using IBM Cloud to create IBM Z RHEL instances, you will also need to specifiy enviroment variables `RHEL_SUBSCRIPTION_USERNAME` and `RHEL_SUBSCRIPTION_PASSWORD` to register your RHEL system.

To create your own, for dev the format should be:

```yaml
---
quay_username: "<quay username>"
quay_password: "<quay_password>"
```

You can also encrypt this file using ansible-vault, should you wish to password
protect it, though you will need to input the password every time it is used.

```bash
$ ansible-vault encrypt secrets.yml
```

This will require some changes to any `ansible-playbook` commands, as you
need to provide a password. Add `--ask-vault-pass` to any commands.

### Available Roles

#### create-vm

This role allows a playbook to create a VM on GCP. It is expected to run on
a host that has the GCP SDK available (which is normally localhost)

See [create-all-vms](./roles/create-all-vms/tasks/main.yml) for an example of how it's used.

#### create-all-vms

Higher level role which creates all VMs from a given list. Calls into create
VM for the heavy lifting.

See [integration-tests.yml](./integration-tests.yml) or [benchmarks.yml](./benchmarks.yml) for
examples of how it's used.

#### provision-vm

This role will setup a VM with docker and get it ready for testing. It includes
platform-specific logic, as well as bootstrapping of python/ansible on Flatcar, which
does not have python installed by default.

See [vm-lifecycle.yml](./vm-lifecycle.yml) for an example of how it's used.

#### destroy-vm

This role will delete a VM from GCP, based on its instance name.

See [vm-lifecycle.yml](./vm-lifecycle.yml) for an example of how it's used.

#### run-test-target

Handles running a make target in the [integration-tests](../integration-tests) directory
in the repository. Will attempt to run the target for eBPF collection
except in cases where a VM is designated for a specific collection method (via the vm_collection_method label)

See [integration-tests.yml](./integration-tests.yml) or [benchmarks.yml](./benchmarks.yml) for
examples of how it's used.

### Layout

The structure of subdirectories and files largely conform to a common [ansible
directory format](https://docs.ansible.com/ansible/2.8/user_guide/playbooks_best_practices.html#content-organization).

| Directory  | Purpose                                            |
| ---------  | -------------------------------------------------- |
| roles      | Common sets of tasks that can be used by playbooks |
| group_vars | Variables for specific groups (or all groups), the filename is the group name. e.g. platform_flatcar.yml affects the platform_flatcar group |
| ci, dev    | These are [inventory directories](https://docs.ansible.com/ansible/latest/user_guide/intro_dynamic_inventory.html#using-inventory-directories-and-multiple-inventory-sources) that contain different variables based on where the playbooks are running. |
| tasks      | Yaml files that contain task lists |
| .          | The root of this directory is where playbooks should exist |

To find VM definitions see [group_vars/all.yml](./group_vars/all.yml)

[^1]: there should be no CI related functionality within this directory
      outside the `ci/` inventory.
