# Usage:
#   make debug
#   make release
#   make clean

CMAKE ?= cmake
BUILD_ROOT ?= Output
APP_NAME := run
BUILD_DIR := $(BUILD_ROOT)

ifeq ($(OS),Windows_NT)
    GENERATOR ?= MinGW Makefiles
    MKDIR = mkdir
    RM = rmdir /S /Q
else
    GENERATOR ?= Unix Makefiles
    MKDIR = mkdir -p
    RM = rm -rf
endif

.PHONY: all debug release clean help

all: release

help:
	@echo "Usage:"
	@echo "  make debug    - build Debug configuration"
	@echo "  make release  - build Release configuration"
	@echo "  make clean    - remove Output directory"
	@echo "  make help     - show this help"

$(BUILD_ROOT):
	$(MKDIR) "$(BUILD_ROOT)"

debug: $(BUILD_ROOT)
	$(CMAKE) -S . -B "$(BUILD_DIR)/debug" -DCMAKE_BUILD_TYPE=Debug -G "$(GENERATOR)"
	$(CMAKE) --build "$(BUILD_DIR)/debug" --config Debug
	@echo "Debug build completed"

release: $(BUILD_ROOT)
	$(CMAKE) -S . -B "$(BUILD_DIR)/release" -DCMAKE_BUILD_TYPE=Release -G "$(GENERATOR)"
	$(CMAKE) --build "$(BUILD_DIR)/release" --config Release
	@echo "Release build completed"

clean:
	$(RM) "$(BUILD_ROOT)"
	@echo "Cleaned Output directory"
