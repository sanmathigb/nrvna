APP_NAME := bckbrnr
BUILD_DIR := .build
APP_DIR := $(BUILD_DIR)/$(APP_NAME).app
CONTENTS := $(APP_DIR)/Contents
MACOS := $(CONTENTS)/MacOS
RESOURCES := $(CONTENTS)/Resources
BIN := $(RESOURCES)/bin
ENGINE_DIR ?= ../build
DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer

.PHONY: app clean run

app:
	mkdir -p "$(MACOS)" "$(BIN)"
	DEVELOPER_DIR="$(DEVELOPER_DIR)" CLANG_MODULE_CACHE_PATH="$(BUILD_DIR)/ModuleCache" xcrun swiftc -O -parse-as-library \
		-sdk "$$(DEVELOPER_DIR="$(DEVELOPER_DIR)" xcrun --sdk macosx --show-sdk-path)" \
		-Xcc -fmodules-cache-path="$(BUILD_DIR)/ModuleCache" \
		-framework AppKit -framework SwiftUI \
		-o "$(MACOS)/$(APP_NAME)" \
		Sources/Bckbrnr/*.swift
	cp Info.plist "$(CONTENTS)/Info.plist"
	install -m 755 "$(ENGINE_DIR)/nrvnad" "$(BIN)/nrvnad"
	install -m 755 "$(ENGINE_DIR)/wrk" "$(BIN)/wrk"
	install -m 755 "$(ENGINE_DIR)/flw" "$(BIN)/flw"

run: app
	open "$(APP_DIR)"

clean:
	rm -rf "$(BUILD_DIR)"
