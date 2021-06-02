ifndef BASE_PATH
$(error Set a BASE_PATH)
endif
PATH ?= $(PATH):/go/bin

GENERATED_CPP_BASE_PATH := $(BASE_PATH)/generated

# Automatically locate all API and data protos.
# GENERATED_API_XXX and PROTO_API_XXX variables contain standard paths used to
# generate gRPC proto messages, services, and gateways for the API.
PROTO_BASE_PATHS = $(BASE_PATH)/rox-proto/
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

GOOGLEAPIS_DIR := $(BASE_PATH)/googleapis/

PROTOC := protoc

PROTOC_INCLUDES := /usr/local/include $(GOOGLEAPIS_DIR)

GRPC_CPP_PLUGIN := grpc_cpp_plugin

GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTO_DEPS_CPP=$(PROTOC_INCLUDES)

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

generated-proto-srcs: $(GENERATED_CPP_SRCS)
