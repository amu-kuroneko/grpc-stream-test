MAKEFILE_DIR = $(dir $(lastword $(MAKEFILE_LIST)))

PROTOS_DIR = ../protos
SOURCES_DIR = ./stream
PROTO_SOURCES_DIR = stream/pb
BINS_DIR = bins
GOPATH ?= $(MAKEFILE_DIR)

PROTOC = /usr/local/bin/protoc
GO = go
GRPC_GO_PLUGIN = $(GOPATH)/bin/protoc-gen-go

PROTO_SOURCES = $(PROTO_SOURCES_DIR)/stream_test.pb.go
GO_LIBS_DIR = $(GOPATH)/src
GO_LIBS = \
	$(GO_LIBS_DIR)/github.com/golang/protobuf/protoc-gen-go \
	$(GO_LIBS_DIR)/google.golang.org/grpc

EXECUTABLE_FILE = $(BINS_DIR)/stream-server

vpath %.proto $(PROTOS_DIR)

build: goget $(PROTO_SOURCES);
	$(GO) build -o $(EXECUTABLE_FILE) $(SOURCES_DIR)

goget: $(GO_LIBS);

$(GO_LIBS):
	$(GO) get -u $(subst $(GO_LIBS_DIR),,$@)

.PRECIOUS: $(PROTO_SOURCES_DIR)/%.pb.go
$(PROTO_SOURCES_DIR)/%.pb.go: %.proto
	@mkdir -p $(PROTO_SOURCES_DIR)
	$(PROTOC) -I $(PROTOS_DIR) --plugin=protoc-gen-go=$(GRPC_GO_PLUGIN) --go_out=plugins=grpc:stream/pb $<

clean:
	rm -rf $(MAKEFILE_DIR)/src
	rm -rf $(PROTO_SOURCES_DIR)
	rm -rf $(BINS_DIR)

