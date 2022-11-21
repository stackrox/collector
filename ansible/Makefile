ifeq ($(BUILD_TYPE), ci)
	CONTEXT=ci
else
	CONTEXT=dev
endif

.PHONY: list-inventory
list-inventory:
	ansible-inventory -i $(CONTEXT) --list

.PHONY: create-vms
create-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags setup,provision \
		vm-lifecycle.yml

.PHONY: provision-vms
provision-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags provision \
		vm-lifecycle.yml

.PHONY: destroy-vms
destroy-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags teardown \
		vm-lifecycle.yml

.PHONY: integration-tests
integration-tests: secrets.yml
	ansible-playbook \
		-i $(CONTEXT) \
		-e @secrets.yml \
		--tags run-tests \
		integration-tests.yml

.PHONY: benchmarks
benchmarks: secrets.yml
	ansible-playbook -i $(CONTEXT) \
		-e @secrets.yml \
		--tags run-benchmark \
		benchmarks.yml

.PHONY: create-benchmark-vms
create-benchmark-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags setup,provision \
		benchmarks.yml

.PHONY: secrets-config
secrets.yml:
	@echo "---" > secrets.yml
	@echo "quay_username: " $(QUAY_RHACS_ENG_RO_USERNAME) >> secrets.yml
	@echo "quay_password: " $(QUAY_RHACS_ENG_RO_PASSWORD) >> secrets.yml

