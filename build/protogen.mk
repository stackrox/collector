BASE_PATH ?= $(CURDIR)/..
PATH ?= $(PATH):/go/bin

# Automatically locate all API and data protos.
# GENERATED_API_XXX and PROTO_API_XXX variables contain standard paths used to
# generate gRPC proto messages, services, and gateways for the API.
PROTO_BASE_PATHS = $(BASE_PATH)/proto/ $(BASE_PATH)/rox-proto/
ALL_PROTOS = $(shell find $(PROTO_BASE_PATHS) -name '*.proto' 2>/dev/null) \
	$(GOOGLEAPIS_DIR)/google/api/annotations.proto \
	$(GOOGLEAPIS_DIR)/google/api/http.proto
SERVICE_PROTOS = $(filter %_service.proto,$(ALL_PROTOS))

ALL_PROTOS_REL = $(ALL_PROTOS:$(BASE_PATH)/%=%)
SERVICE_PROTOS_REL = $(SERVICE_PROTOS:$(BASE_PATH)/%=%)

GENERATED_CPP_SRCS = \
    $(ALL_PROTOS_REL:%.proto=$(GENERATED_CPP_BASE_PATH)/%.pb.cc) \
    $(ALL_PROTOS_REL:%.proto=$(GENERATED_CPP_BASE_PATH)/%.pb.h) \
    $(SERVICE_PROTOS_REL:%.proto=$(GENERATED_CPP_BASE_PATH)/%.grpc.pb.cc) \
    $(SERVICE_PROTOS_REL:%.proto=$(GENERATED_CPP_BASE_PATH)/%.grpc.pb.h)

##############
## Protobuf ##
##############
# Set some platform variables for protoc.
PROTOC_VERSION := 3.6.1
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
PROTOC_ARCH = linux
endif
ifeq ($(UNAME_S),Darwin)
PROTOC_ARCH = osx
endif

TMP_PATH := $(BASE_PATH)/.protogen-tmp/

PROTOC_ZIP := protoc-$(PROTOC_VERSION)-$(PROTOC_ARCH)-x86_64.zip
PROTOC_FILE := $(TMP_PATH)/$(PROTOC_ZIP)

PROTOC_TMP := $(TMP_PATH)/protoc-tmp/

GOOGLEAPIS_FILE := $(TMP_PATH)/googleapis.zip

GOOGLEAPIS_DIR := $(BASE_PATH)/googleapis/

PROTOC := $(PROTOC_TMP)/bin/protoc

PROTOC_INCLUDES := $(PROTOC_TMP)/include $(GOOGLEAPIS_DIR)

GRPC_CPP_PLUGIN := grpc_cpp_plugin

GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

$(TMP_PATH):
	@echo "+ $@"
	@mkdir -p $@

$(PROTOC_FILE): $(TMP_PATH)
	@echo "+ $@"
	@wget --no-use-server-timestamps -q https://github.com/google/protobuf/releases/download/v$(PROTOC_VERSION)/$(PROTOC_ZIP) -O $@

$(GOOGLEAPIS_FILE): $(TMP_PATH)
	@echo "+ $@"
	@wget --no-use-server-timestamps -q https://github.com/googleapis/googleapis/archive/master.zip -O $@

$(GOOGLEAPIS_DIR): $(GOOGLEAPIS_FILE)
	@echo "+ $@"
	@unzip -q -o -DD -d $(TMP_PATH) $<
	@rm -rf $@
	@mv $(TMP_PATH)/googleapis-master $@

$(PROTOC_TMP)/include: $(PROTOC_TMP)

$(PROTOC): $(PROTOC_TMP)
	@echo "+ $@"
	@chmod a+rx $@

$(PROTOC_TMP): $(PROTOC_FILE) $(TMP_PATH)
	@echo "+ $@"
	@mkdir -p $@
	@unzip -q -o -DD -d $@ $<

PROTO_DEPS_CPP=$(PROTOC) $(PROTOC_INCLUDES)

#######################################################################
## Generate gRPC proto messages, services, and gateways for the API. ##
#######################################################################

$(GENERATED_CPP_BASE_PATH):
	@echo "+ $@"
	@mkdir -p "$@"

SUBDIR = $(firstword $(subst /, ,$(subst $(BASE_PATH)/,,$<)))

$(GENERATED_CPP_BASE_PATH)/%.pb.cc $(GENERATED_CPP_BASE_PATH)/%.pb.h: $(BASE_PATH)/%.proto $(GENERATED_CPP_BASE_PATH) $(PROTO_DEPS_CPP)
	@echo "+ $@"
	@mkdir -p $(GENERATED_CPP_BASE_PATH)/$(SUBDIR)
	@$(PROTOC) \
		$(PROTOC_INCLUDES:%=-I%) \
		$(PROTO_BASE_PATHS:%=-I%) \
		--cpp_out=$(GENERATED_CPP_BASE_PATH)/$(SUBDIR) \
		$(filter $(BASE_PATH)/$(SUBDIR)/%, $(ALL_PROTOS))

$(GENERATED_CPP_BASE_PATH)/%.grpc.pb.cc $(GENERATED_CPP_BASE_PATH)/%.grpc.pb.h: $(BASE_PATH)/%.proto $(GENERATED_CPP_BASE_PATH) $(PROTO_DEPS_CPP)
	@echo "+ $@"
	@mkdir -p $(GENERATED_CPP_BASE_PATH)/$(SUBDIR)
	@$(PROTOC) \
		$(PROTOC_INCLUDES:%=-I%) \
		$(PROTO_BASE_PATHS:%=-I%) \
		--grpc_out=$(GENERATED_CPP_BASE_PATH)/$(SUBDIR) \
		--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) \
		$(filter $(BASE_PATH)/$(SUBDIR)/%, $(ALL_PROTOS))


# Clean things that we use to generate protobufs
.PHONY: clean-protogen-artifacts
clean-protogen-artifacts:
	@echo "+ $@"
	@rm -rf $(TMP_PATH)
