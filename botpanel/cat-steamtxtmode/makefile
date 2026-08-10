CXX ?= g++
SHELL := $(if $(wildcard /bin/sh),/bin/sh,$(if $(wildcard /bin/bash),/bin/bash,/usr/bin/bash))
BUILD_MAKEFILE := $(lastword $(MAKEFILE_LIST))
REPO_ROOT ?= ../..
OUT_DIR ?= bin
LIBDIR64 ?= /usr/lib/$(shell gcc -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
LIBDIR32 ?= /usr/lib/$(shell gcc -m32 -print-multiarch 2>/dev/null || echo i386-linux-gnu)
SONAME = libcatsteamtxtmode.so
SRC_DIR = src
SRC = $(wildcard $(SRC_DIR)/*.cpp)

CXXFLAGS ?= -std=c++20 -O2 -g -fPIC -fvisibility=hidden -Wall -Wextra -fmessage-length=0
CXXFLAGS += -I$(SRC_DIR)
LDFLAGS = -shared -Wl,--no-undefined -Wl,-soname,$(SONAME)
LDLIBS = -ldl

.PHONY: all lib64 lib32 install clean

all: lib64 lib32

lib64: $(OUT_DIR)/lib64/$(SONAME)
lib32: $(OUT_DIR)/lib/$(SONAME)

$(OUT_DIR)/lib64/$(SONAME): $(SRC) $(BUILD_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) -m64 $(CXXFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(OUT_DIR)/lib/$(SONAME): $(SRC) $(BUILD_MAKEFILE)
	@mkdir -p $(dir $@)
	$(CXX) -m32 $(CXXFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

install: all
	install -d -m 0755 "$(LIBDIR64)" "$(LIBDIR32)"
	install -m 0755 $(OUT_DIR)/lib64/$(SONAME) "$(LIBDIR64)/"
	install -m 0755 $(OUT_DIR)/lib/$(SONAME) "$(LIBDIR32)/"
	ldconfig || true

clean:
	rm -rf "$(OUT_DIR)"
