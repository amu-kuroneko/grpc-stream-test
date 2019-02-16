export PKG_CONFIG_PATH=/opt/antism/lib64/pkgconfig

MAKEFILE_DIR = $(dir $(lastword $(MAKEFILE_LIST)))

PROTOS_DIR = ../protos
SOURCES_DIR = stream
PROTO_SOURCES_DIR = stream/pb
OBJECTS_DIR = objs
BINS_DIR = bins

PROTOC = /usr/local/bin/protoc
GRPC_CPP_PLUGIN = /usr/local/bin/grpc_cpp_plugin
GRPC_CPP_PLUGIN_DIR ?= `which $(GRPC_CPP_PLUGIN)`

GCC = g++
CPPFLAGS += -I$(MAKEFILE_DIR) -pthread
CXXFLAGS += -std=c++17 -O2 -g
LDFLAGS += \
	`pkg-config --libs grpc++ grpc` \
	-lgrpc++_reflection \
	-lprotobuf \
	-lpthread \
	-lboost_system \
	-lboost_thread-mt \
	-ldl

EXECUTABLE_FILES = \
	$(BINS_DIR)/stream_client \
	$(BINS_DIR)/async_stream_client
OUTPUTS_DIR = $(OBJECTS_DIR)
TARGET_DIR = $(BINS_DIR)

PROTO_OBJECTS = \
	$(OUTPUTS_DIR)/pb/stream_test.pb.o \
	$(OUTPUTS_DIR)/pb/stream_test.grpc.pb.o

BUILD_OBJECTS = $(PROTO_OBJECTS)

vpath %.proto $(PROTOS_DIR)

build: $(EXECUTABLE_FILES);

$(BINS_DIR)/%: $(BUILD_OBJECTS) $(OUTPUTS_DIR)/%.o
	@mkdir -p $(TARGET_DIR)
	$(GCC) $^ $(CXXFLAGS) $(LDFLAGS) -o $@

$(OUTPUTS_DIR)/%.o: $(SOURCES_DIR)/%.cc
	@mkdir -p $(dir $@)
	$(GCC) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

.PRECIOUS: $(PROTO_SOURCES_DIR)/%.grpc.pb.cc
$(PROTO_SOURCES_DIR)/%.grpc.pb.cc: %.proto
	@mkdir -p $(PROTO_SOURCES_DIR)
	$(PROTOC) -I $(PROTOS_DIR) --grpc_out=$(PROTO_SOURCES_DIR) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_DIR) $<

.PRECIOUS: $(PROTO_SOURCES_DIR)/%.pb.cc
$(PROTO_SOURCES_DIR)/%.pb.cc: %.proto
	@mkdir -p $(PROTO_SOURCES_DIR)
	$(PROTOC) -I $(PROTOS_DIR) --cpp_out=$(PROTO_SOURCES_DIR) $<

clean:
	rm -rf $(OBJECTS_DIR)
	rm -rf $(PROTO_SOURCES_DIR)
	rm -rf $(BINS_DIR)
