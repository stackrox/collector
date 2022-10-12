ifeq ($(BUILD_TYPE), ci)
	CONTEXT=ci
else
	CONTEXT=dev
endif

.PHONY: list-inventory
list-inventory:
	ansible-inventory -i $(CONTEXT)/inventory_gcp.yml --list

.PHONY: integration-tests-teardown
integration-tests-teardown:
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @$(CONTEXT)/vars.yml \
		--tags teardown \
		integration-tests.yml

.PHONY: integration-tests-setup
integration-tests-setup:
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @$(CONTEXT)/vars.yml \
		--tags setup \
		integration-tests.yml

.PHONY: integraion-tests-provision
integration-tests-provision:
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @$(CONTEXT)/vars.yml \
		--tags provision \
		integration-tests.yml

.PHONY: integration-tests
integration-tests: secrets.yml
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @secrets.yml \
		-e @$(CONTEXT)/vars.yml \
		--tags run-tests \
		integration-tests.yml

.PHONY: secrets-config
secrets.yml:
	@echo "---" > secrets.yml
	@echo "quay_username: " $(QUAY_RHACS_ENG_RO_USERNAME) >> secrets.yml
	@echo "quay_password: " $(QUAY_RHACS_ENG_RO_PASSWORD) >> secrets.yml

