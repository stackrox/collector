all: create-vms run-tests teardown-vms

.PHONY: create-vms
create-vms:
	vagrant up

.PHONY: run-tests
run-tests:
	ansible-playbook -i .vagrant/provisioners/inventory/vagrant_ansible_inventory \
		-e @secrets.enc --ask-vault-pass \
		collector-integration-tests.yml

.PHONY: teardown-vms
teardown-vms:
	vagrant destroy -f


