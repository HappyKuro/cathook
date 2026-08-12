CXX ?= g++
ARCH ?= 32

ifeq ($(ARCH),32)
OUTPUT_DIR := bin/lib64
ARCH_FLAGS := -m32
ELF_CHECK := ELF 32-bit
ELF_CLASS := ELF32
else ifeq ($(ARCH),64)
OUTPUT_DIR := bin/libx64
ARCH_FLAGS :=
ELF_CHECK := ELF 64-bit
ELF_CLASS := ELF64
else
$(error ARCH must be 32 or 64)
endif

TARGET := $(OUTPUT_DIR)/libcatsteamtxtmode.so
SOURCES := $(wildcard src/*.cpp)

CXXFLAGS ?= -O2 -pipe -std=c++23 -fPIC -fvisibility=hidden -Wall -Wextra -Wpedantic
LDFLAGS ?= -shared -Wl,-z,relro,-z,now -Wl,-z,noexecstack
LDLIBS := -ldl -pthread

.PHONY: all clean verify

all: $(TARGET)

$(TARGET): $(SOURCES) Makefile

	@mkdir -p $(dir $@)
	$(CXX) $(ARCH_FLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)

verify: $(TARGET)
	file $(TARGET) | grep -q '$(ELF_CHECK)'
	readelf -h $(TARGET) | grep -q 'Class:.*$(ELF_CLASS)'
	nm -D $(TARGET) | grep -q ' dlopen$$'

clean:
	rm -f bin/lib64/libcatsteamtxtmode.so bin/libx64/libcatsteamtxtmode.so
