all: create-vms run-tests teardown-vms

.PHONY: create-vms
create-vms:
	vagrant up

.PHONY: run-tests
run-tests: build-tests
	ansible-playbook -i .vagrant/provisioners/ansible/inventory/vagrant_ansible_inventory \
		-e @secrets.enc --ask-vault-pass \
		collector-integration-tests.yml

.PHONY: teardown-vms
teardown-vms:
	vagrant destroy --force

.PHONY: build-tests
build-tests: $(shell find ../ -type f -name '*.go')
	GOOS=linux GOARCH=amd64 go test -c ../

