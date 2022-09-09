
.PHONY: list-inventory
list-inventory:
	ansible-inventory -i inventory_ci_gcp.yml --list

.PHONY: teardown
teardown:
	ansible-playbook -i inventory_ci_gcp.yml playbooks/teardown.yml

.PHONY: setup
setup:
	ansible-playbook -i inventory_ci_gcp.yml playbooks/setup.yml

.PHONY: provision
provision:
	ansible-playbook -i inventory_ci_gcp.yml -e @secrets.enc --ask-vault-pass playbooks/provision-vms.yml

.PHONY: run
run:
	ansible-playbook -i inventory_ci_gcp.yml -e @secrets.enc --ask-vault-pass main.yml

.PHONY: run-tests
run-tests:
	ansible-playbook -i inventory_ci_gcp.yml -e @secrets.enc --ask-vault-pass playbooks/run-tests.yml

define runner
run-$(1):
	VM_TYPE=$(1) ansible-playbook -i inventory_ci_gcp.yml -e @secrets.enc --ask-vault-pass main.yml
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
