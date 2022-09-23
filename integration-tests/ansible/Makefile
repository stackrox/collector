ifeq ($(BUILD_TYPE), ci)
	CONTEXT=ci
else
	CONTEXT=dev
endif

.PHONY: list-inventory
list-inventory:
	ansible-inventory -i $(CONTEXT)/inventory_gcp.yml --list

.PHONY: teardown
teardown:
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @$(CONTEXT)/vars.yml \
		teardown.yml

.PHONY: setup
setup:
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @$(CONTEXT)/vars.yml \
		setup.yml

.PHONY: provision
provision:
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @$(CONTEXT)/vars.yml \
		playbooks/provision-vms.yml

.PHONY: run
run: secrets.yml
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @secrets.yml \
		-e @$(CONTEXT)/vars.yml \
		main.yml

.PHONY: run-tests
run-tests: secrets.yml
	ansible-playbook \
		-i $(CONTEXT)/inventory_gcp.yml \
		-e @secrets.yml \
		-e @$(CONTEXT)/vars.yml \
		playbooks/run-tests.yml

.PHONY: secrets-config
secrets.yml:
	@echo "---" > secrets.yml
	@echo "quay_username: " $(QUAY_RHACS_ENG_RO_USERNAME) >> secrets.yml
	@echo "quay_password: " $(QUAY_RHACS_ENG_RO_PASSWORD) >> secrets.yml

define runner
run-$(1):
	export VM_TYPE=$(1)
	ansible-playbook -i $(INVENTORY) -e secrets.yml main.yml
endef

$(eval $(call runner,rhel))
$(eval $(call runner,rhel-sap))
$(eval $(call runner,cos))
$(eval $(call runner,ubuntu-os))
$(eval $(call runner,ubuntu-os-pro))
$(eval $(call runner,sles))
$(eval $(call runner,flatcar))
$(eval $(call runner,fedora-coreos))
$(eval $(call runner,garden-linux))
