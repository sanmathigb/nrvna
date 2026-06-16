APP_NAME := bckbrnr
BUILD_DIR := .build
APP_DIR := $(BUILD_DIR)/$(APP_NAME).app
CONTENTS := $(APP_DIR)/Contents
MACOS := $(CONTENTS)/MacOS
DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer

.PHONY: app clean run

app:
	mkdir -p "$(MACOS)"
	DEVELOPER_DIR="$(DEVELOPER_DIR)" CLANG_MODULE_CACHE_PATH="$(BUILD_DIR)/ModuleCache" xcrun swiftc -O -parse-as-library \
		-sdk "$$(DEVELOPER_DIR="$(DEVELOPER_DIR)" xcrun --sdk macosx --show-sdk-path)" \
		-Xcc -fmodules-cache-path="$(BUILD_DIR)/ModuleCache" \
		-framework AppKit -framework SwiftUI \
		-o "$(MACOS)/$(APP_NAME)" \
		Sources/Async/*.swift
	cp Info.plist "$(CONTENTS)/Info.plist"

run: app
	launchctl setenv ASYNC_ENGINE_DIR "$$(cd .. && pwd)/build"
	open "$(APP_DIR)"

clean:
	rm -rf "$(BUILD_DIR)"
