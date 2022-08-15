check_defined = \
    $(strip $(foreach 1,$1, \
        $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
        $(error Undefined $1$(if $2, ($2))$(if $(value @), \
                required by target `$@')))

.PHONY: create-all-vms
create-all-vms:
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

.PHONY: create-vm
create-vm:
	@:$(call check_defined, VM, vm name)
	vagrant up $(VM)
