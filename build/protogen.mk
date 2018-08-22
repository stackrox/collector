BASE_PATH ?= $(CURDIR)/..

# Automatically locate all API and data protos.
PROTO_BASE_PATH := $(BASE_PATH)/proto
API_SERVICE_PROTOS := $(shell find $(PROTO_BASE_PATH)/api -name '*.proto')
DATA_PROTOS := $(shell find $(PROTO_BASE_PATH)/data -name '*.proto')

# DATA_PROTOS, relative to $(BASE_PATH)
DATA_PROTOS_REL := $(DATA_PROTOS:$(BASE_PATH)/%=%)

# GENERATED_API_XXX and PROTO_API_XXX variables contain standard paths used to
# generate gRPC proto messages, services, and gateways for the API.
GENERATED_BASE_PATH ?= $(BASE_PATH)/pkg/generated
GENERATED_CPP_BASE_PATH ?= $(BASE_PATH)/pkg/generated-cpp

# Files generated from DATA_PROTOS
GENERATED_DATA_SRCS := $(DATA_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_BASE_PATH)/%.pb.go)
GENERATED_CPP_DATA_SRCS := \
	$(DATA_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_CPP_BASE_PATH)/%.pb.cc) \
	$(DATA_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_CPP_BASE_PATH)/%.pb.h)

# Files generated from API_PROTOS
GENERATED_API_SRCS := $(API_SERVICE_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_BASE_PATH)/%.pb.go)
GENERATED_API_GW_SRCS := $(API_SERVICE_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_BASE_PATH)/%.pb.gw.go)
GENERATED_API_VALIDATOR_SRCS := $(API_SERVICE_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_BASE_PATH)/%.validator.pb.go)

# Docs/Swagger
GENERATED_DOC_BASE_PATH := docs/generated
V1_DOC_API_SERVICES ?=
V1_DOC_API_SERVICE_PROTOS = $(V1_DOC_API_SERVICES:%=$(PROTO_BASE_PATH)/api/v1/%.proto)

GENERATED_V1_API_SWAGGER_SPECS = $(V1_DOC_API_SERVICE_PROTOS:$(BASE_PATH)/%.proto=$(GENERATED_DOC_BASE_PATH)/%.swagger.json)
SWAGGER_PATH := $(GOPATH)/bin/swagger

# Generated API docs - this is fixed to api/v1, we don't generated docs for anything else.
GENERATED_V1_DOC_PATH = $(GENERATED_DOC_BASE_PATH)/proto/api/v1
MERGED_V1_API_SWAGGER_SPEC = $(GENERATED_V1_DOC_PATH)/swagger.json
GENERATED_V1_API_DOCS = $(GENERATED_V1_DOC_PATH)/reference

# The --go_out=M... argument specifies the go package to use for an imported proto file. Here, we instruct protoc-gen-go
# to import the go source for proto file $(BASE_PATH)/<path>/*.proto to
# "bitbucket.org/stack-rox/stackrox/pkg/generated/<path>".
M_ARGS = $(foreach proto,$(DATA_PROTOS_REL),M$(proto)=bitbucket.org/stack-rox/stackrox/pkg/generated/$(patsubst %/,%,$(dir $(proto))))

# Hack: there's no straightforward way to escape a comma in a $(subst ...) command, so we have to resort to this little
# trick.
null :=
space := $(null) $(null)
comma := ,

M_ARGS_STR := $(subst $(space),$(comma),$(strip $(M_ARGS)))

##############
## Protobuf ##
##############
# Set some platform variables for protoc.
PROTOC_VERSION := 3.5.0
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
PROTOC_ARCH = linux
endif
ifeq ($(UNAME_S),Darwin)
PROTOC_ARCH = osx
endif

PROTOC_ZIP := protoc-$(PROTOC_VERSION)-$(PROTOC_ARCH)-x86_64.zip
PROTOC_FILE := $(BASE_PATH)/$(PROTOC_ZIP)

PROTOC_TMP := $(BASE_PATH)/protoc-tmp

PROTOC := $(PROTOC_TMP)/bin/protoc

PROTOC_INCLUDES := $(PROTOC_TMP)/include/google

PROTOC_GEN_GO := $(GOPATH)/src/github.com/golang/protobuf/protoc-gen-go

PROTOC_GEN_GO_BIN := $(GOPATH)/bin/protoc-gen-gofast

$(GOPATH)/src/github.com/gogo/protobuf/types:
	@echo "+ $@"
	@$(BASE_PATH)/scripts/go-get-version.sh github.com/gogo/protobuf/types v1.0.0

$(PROTOC_GEN_GO_BIN): $(GOPATH)/src/github.com/gogo/protobuf/types
	@echo "+ $@"
	@$(BASE_PATH)/scripts/go-get-version.sh github.com/gogo/protobuf/protoc-gen-gofast v1.0.0

GOGO_M_STR := Mgoogle/protobuf/any.proto=github.com/gogo/protobuf/types,Mgoogle/protobuf/duration.proto=github.com/gogo/protobuf/types,Mgoogle/protobuf/struct.proto=github.com/gogo/protobuf/types,Mgoogle/protobuf/timestamp.proto=github.com/gogo/protobuf/types,Mgoogle/protobuf/wrappers.proto=github.com/gogo/protobuf/types

$(GOPATH)/src/github.com/golang/protobuf/protoc-gen-go:
	@echo "+ $@"
	@$(BASE_PATH)/scripts/go-get-version.sh github.com/golang/protobuf/protoc-gen-go v1.1.0

$(PROTOC_FILE):
	@echo "+ $@"
	@wget -q https://github.com/google/protobuf/releases/download/v$(PROTOC_VERSION)/$(PROTOC_ZIP) -O $(PROTOC_FILE)

$(PROTOC_INCLUDES): $(PROTOC_TMP)
	@echo "+ $@"

$(PROTOC): $(PROTOC_TMP)
	@echo "+ $@"

$(PROTOC_TMP): $(PROTOC_FILE)
	@echo "+ $@"
	@mkdir $(PROTOC_TMP)
	@unzip -q -d $(PROTOC_TMP) $(PROTOC_FILE)

.PHONY: proto-fmt
proto-fmt:
	@go get github.com/ckaznocha/protoc-gen-lint
	@echo "Checking for proto style errors"
	@$(PROTOC) \
		--proto_path=$(BASE_PATH) \
		--lint_out=. \
		$(DATA_PROTOS)
	@$(PROTOC) \
		-I$(PROTOC_INCLUDES) \
		-I$(GOPATH)/src \
		-I$(GOPATH)/src/github.com/gogo/protobuf/protobuf \
		-I$(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--lint_out=. \
		--proto_path=$(BASE_PATH) \
		$(API_SERVICE_PROTOS)

PROTO_DEPS_CPP=$(PROTOC) $(PROTOC_INCLUDES)
PROTO_DEPS=$(PROTOC_GEN_GO) $(PROTO_DEPS_CPP)

###############
## Utilities ##
###############

.PHONY: printdocs
printdocs:
	@echo $(GENERATED_API_DOCS)

.PHONY: printswaggers
printswaggers:
	@echo $(GENERATED_V1_API_SWAGGER_SPECS)

.PHONY: printsrcs
printsrcs:
	@echo $(GENERATED_SRCS)

.PHONY: printapisrcs
printapisrcs:
	@echo $(GENERATED_API_SRCS)

.PHONY: printgwsrcs
printgwsrcs:
	@echo $(GENERATED_API_GW_SRCS)

.PHONY: printvalidatorsrcs
printvalidatorsrcs:
	@echo $(GENERATED_API_VALIDATOR_SRCS)

.PHONY: printprotos
printprotos:
	@echo $(API_SERVICE_PROTOS)

#######################################################################
## Generate gRPC proto messages, services, and gateways for the API. ##
#######################################################################

PROTOC_GEN_GRPC_GATEWAY := $(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway

PROTOC_GEN_GOVALIDATORS := $(GOPATH)/src/github.com/mwitkow/go-proto-validators

$(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway:
	@echo "+ $@"
	@$(BASE_PATH)/scripts/go-get-version.sh google.golang.org/genproto/googleapis 7bb2a897381c9c5ab2aeb8614f758d7766af68ff --skip-install
	@$(BASE_PATH)/scripts/go-get-version.sh github.com/grpc-ecosystem/grpc-gateway/protoc-gen-grpc-gateway/... c2b051dd2f71ce445909aab7b28479fd84d00086
	@$(BASE_PATH)/scripts/go-get-version.sh github.com/grpc-ecosystem/grpc-gateway/protoc-gen-swagger/... c2b051dd2f71ce445909aab7b28479fd84d00086

$(GOPATH)/src/github.com/mwitkow/go-proto-validators:
	@echo "+ $@"
	@go get -u github.com/mwitkow/go-proto-validators/protoc-gen-govalidators

$(GENERATED_BASE_PATH):
	@echo "+ $@"
	@mkdir -p "$@"

$(GENERATED_CPP_BASE_PATH):
	@echo "+ $@"
	@mkdir -p "$@"

$(GENERATED_DOC_BASE_PATH):
	@echo "+ $@"
	@mkdir -p "$@"

# Generate all of the proto messages and gRPC services with one invocation of
# protoc when any of the .pb.go sources don't exist or when any of the .proto
# files change.
$(GENERATED_BASE_PATH)/%.pb.go: $(BASE_PATH)/%.proto $(GENERATED_BASE_PATH) $(PROTO_DEPS) $(PROTOC_GEN_GRPC_GATEWAY) $(PROTOC_GEN_GOVALIDATORS) $(API_SERVICE_PROTOS) $(DATA_PROTOS) $(PROTOC_GEN_GO_BIN)
	@echo "+ $@"
	@$(PROTOC) \
		-I$(PROTOC_INCLUDES) \
		-I$(GOPATH)/src \
		-I$(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--proto_path=$(BASE_PATH) \
		--gofast_out=$(GOGO_M_STR),$(M_ARGS_STR),plugins=grpc:$(GENERATED_BASE_PATH) \
		$(dir $<)/*.proto

$(GENERATED_CPP_BASE_PATH)/%.pb.cc $(GENERATED_CPP_BASE_PATH)/%.pb.h: $(BASE_PATH)/%.proto $(GENERATED_CPP_BASE_PATH) $(PROTO_DEPS)
	@echo "+ $@"
	@$(PROTOC) \
		-I$(PROTOC_INCLUDES) \
		-I$(BASE_PATH) \
		--cpp_out=$(GENERATED_CPP_BASE_PATH) \
		$<

# Generate all of the reverse-proxies (gRPC-Gateways) with one invocation of
# protoc when any of the .pb.gw.go sources don't exist or when any of the
# .proto files change.
$(GENERATED_BASE_PATH)/%.pb.gw.go: $(BASE_PATH)/%.proto $(GENERATED_BASE_PATH)/%.pb.go
	@echo "+ $@"
	@mkdir -p $(dir $@)
	@$(PROTOC) \
		-I$(PROTOC_INCLUDES) \
		-I$(GOPATH)/src \
		-I$(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--proto_path=$(BASE_PATH) \
		--grpc-gateway_out=logtostderr=true:$(GENERATED_BASE_PATH) \
		$(dir $<)/*.proto
	@for f in $(patsubst $(dir $<)/%.proto, $(dir $@)/%.pb.gw.go, $(wildcard $(dir $<)/*.proto)); do \
    	test -f $$f || echo package $(subst -,_,$(notdir $(patsubst %/, %, $(dir $<)))) >$$f; \
    done

# Generate all of the validator sources with one invocation of protoc
# when any of the .validator.pb.go sources don't exist or when any of the
# .proto files change.
$(GENERATED_BASE_PATH)/%.validator.pb.go: $(BASE_PATH)/%.proto $(GENERATED_BASE_PATH)/%.pb.go
	@echo "+ $@"
	@mkdir -p $(dir $@)
	@$(PROTOC) \
		-I$(PROTOC_INCLUDES) \
		-I$(GOPATH)/src \
		-I$(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--proto_path=$(BASE_PATH) \
		--govalidators_out=$(M_ARGS_STR):$(GENERATED_BASE_PATH) \
		$(dir $<)/*.proto
	@for f in $(patsubst $(dir $<)/%.proto, $(dir $@)/%.validator.pb.go, $(wildcard $(dir $<)/*.proto)); do \
		test -f $$f || echo package $(subst -,_,$(notdir $(patsubst %/, %, $(dir $<)))) >$$f; \
	done

# Generate all of the swagger specifications with one invocation of protoc
# when any of the .swagger.json sources don't exist or when any of the
# .proto files change.
$(GENERATED_DOC_BASE_PATH)/%.swagger.json: $(BASE_PATH)/%.proto $(PROTO_DEPS) $(PROTOC_GEN_GRPC_GATEWAY) $(PROTOC_GEN_GOVALIDATORS) $(GENERATED_DOC_BASE_PATH) $(API_SERVICE_PROTOS)
	@echo "+ $@"
	@$(PROTOC) \
		-I$(PROTOC_INCLUDES) \
		-I$(GOPATH)/src \
		-I$(GOPATH)/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--proto_path=$(BASE_PATH) \
		--swagger_out=logtostderr=true:$(GENERATED_DOC_BASE_PATH) \
		$(dir $<)/*.proto

$(SWAGGER_PATH):
	@echo "+ $@"
	@go get -u github.com/go-swagger/go-swagger/cmd/swagger

# Generate the docs from the merged swagger specs.
$(MERGED_V1_API_SWAGGER_SPEC): $(SWAGGER_PATH) $(BASE_PATH)/scripts/mergeswag.sh $(GENERATED_V1_API_SWAGGER_SPECS)
	@echo "+ $@"
	$(BASE_PATH)/scripts/mergeswag.sh $(GENERATED_V1_DOC_PATH)
	@$(SWAGGER_PATH) validate $@

# Generate the docs from the merged swagger specs.
$(GENERATED_V1_API_DOCS): $(MERGED_V1_API_SWAGGER_SPEC)
	@echo "+ $@"
	docker run --user $(shell id -u) --rm -v $(CURDIR)/docs:/tmp/docs swaggerapi/swagger-codegen-cli generate -l html2 -i /tmp/$< -o /tmp/$@


# Clean things that we use to generate protobufs
.PHONY: clean-protogen-artifacts
clean-protogen-artifacts:
	@echo "+ $@"
	@rm -rf $(GOPATH)/src/github.com/grpc-ecosystem
	@rm -rf $(GOPATH)/src/github.com/golang/protobuf
	@rm -rf $(GOPATH)/src/google.golang.org/genproto
	@rm -f $(GOPATH)/bin/protoc-gen-grpc-gateway
	@rm -f $(GOPATH)/bin/protoc-gen-go
	@rm -f $(GOPATH)/bin/protoc-gen-gofast
	@rm -rf $(GOPATH)/src/github.com/gogo/protobuf
	@rm -rf $(PROTOC_TMP)
	@rm -f $(PROTOC_FILE)
