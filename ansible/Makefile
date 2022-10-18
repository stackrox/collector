ifeq ($(BUILD_TYPE), ci)
	CONTEXT=ci
else
	CONTEXT=dev
endif

.PHONY: install-deps
install-deps:
	ansible-galaxy install --roles-path=./galaxy-roles -r requirements.yml

.PHONY: list-inventory
list-inventory:
	ansible-inventory -i $(CONTEXT) --list

.PHONY: integration-tests-teardown
integration-tests-teardown:
	ansible-playbook \
		-i $(CONTEXT) \
		--tags teardown \
		integration-tests.yml

.PHONY: integration-tests-setup
integration-tests-setup:
	ansible-playbook \
		-i $(CONTEXT) \
		--tags setup \
		integration-tests.yml

.PHONY: integraion-tests-provision
integration-tests-provision:
	ansible-playbook \
		-i $(CONTEXT) \
		--tags provision \
		integration-tests.yml

.PHONY: integration-tests
integration-tests: secrets.yml
	ansible-playbook \
		-i $(CONTEXT) \
		-e @secrets.yml \
		--tags run-tests \
		integration-tests.yml

.PHONY: secrets-config
secrets.yml:
	@echo "---" > secrets.yml
	@echo "quay_username: " $(QUAY_RHACS_ENG_RO_USERNAME) >> secrets.yml
	@echo "quay_password: " $(QUAY_RHACS_ENG_RO_PASSWORD) >> secrets.yml

