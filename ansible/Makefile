ifeq ($(BUILD_TYPE), ci)
	CONTEXT=ci
else
	CONTEXT=dev
endif

JOB_ID ?= $(USER)

.PHONY: list-inventory
list-inventory:
	ansible-inventory -i $(CONTEXT) --list

.PHONY: create-vms
create-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags setup,provision \
		-e job_id="$(JOB_ID)" \
		vm-lifecycle.yml

.PHONY: provision-vms
provision-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags provision \
		-e job_id="$(JOB_ID)" \
		vm-lifecycle.yml

.PHONY: destroy-vms
destroy-vms:
	ansible-playbook -i $(CONTEXT) \
		--tags teardown \
		-e job_id="$(JOB_ID)" \
		vm-lifecycle.yml

.PHONY: image-upload
image-upload:
	ansible-playbook -i $(CONTEXT) -e job_id="$(JOB_ID)" image-upload.yml

.PHONY: integration-tests
integration-tests: secrets.yml
	ansible-playbook \
		-i $(CONTEXT) \
		-e job_id="$(JOB_ID)" \
		-e @secrets.yml \
		--tags run-tests \
		integration-tests.yml

.PHONY: benchmarks
benchmarks: secrets.yml
	ansible-playbook -i $(CONTEXT) \
		-e job_id="$(JOB_ID)" \
		-e @secrets.yml \
		--tags run-benchmarks \
		benchmarks.yml

.PHONY: create-benchmark-vms
create-benchmark-vms:
	ansible-playbook -i $(CONTEXT) \
		-e job_id="$(JOB_ID)" \
		--tags setup,provision \
		benchmarks.yml

.PHONY: create-ci-vms
create-ci-vms:
	ansible-playbook -i $(CONTEXT) \
		-e job_id="$(JOB_ID)" \
		--tags setup,provision \
		integration-tests.yml

.PHONY: vm-local-dev
vm-local-dev:
	ansible-playbook -i $(CONTEXT) \
		-e job_id="$(JOB_ID)" \
		local-development.yml

.PHONY: secrets-config
secrets.yml:
	@echo "---" > secrets.yml
	@echo "quay_username: " $(QUAY_RHACS_ENG_RO_USERNAME) >> secrets.yml
	@echo "quay_password: " $(QUAY_RHACS_ENG_RO_PASSWORD) >> secrets.yml

