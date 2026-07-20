APP_NAME := bckbrnr
BUILD_DIR := .build
APP_DIR := $(BUILD_DIR)/$(APP_NAME).app
CONTENTS := $(APP_DIR)/Contents
MACOS := $(CONTENTS)/MacOS
RESOURCES := $(CONTENTS)/Resources
BIN := $(RESOURCES)/bin
ENGINE_DIR ?= $(BUILD_DIR)/engine
MACOSX_DEPLOYMENT_TARGET ?= 13.3
ARCH ?= $(shell uname -m)
SWIFT_TARGET ?= $(ARCH)-apple-macosx$(MACOSX_DEPLOYMENT_TARGET)
# Use the active toolchain (xcode-select) rather than assuming Xcode's location;
# fall back to the default install path if xcode-select isn't set up.
DEVELOPER_DIR ?= $(shell xcode-select -p 2>/dev/null || echo /Applications/Xcode.app/Contents/Developer)

.PHONY: app bundle engine clean integration-test run test

engine:
	cmake -S ../.. -B "$(ENGINE_DIR)" \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DCMAKE_OSX_DEPLOYMENT_TARGET="$(MACOSX_DEPLOYMENT_TARGET)" \
		-DGGML_NATIVE=OFF \
		-DLLAMA_CURL=OFF \
		-DLLAMA_OPENSSL=OFF
	cmake --build "$(ENGINE_DIR)" -j4 --target nrvnad wrk flw

app: engine
	$(MAKE) bundle ENGINE_DIR="$(ENGINE_DIR)"

# Assemble an app around an engine that was already built. Release CI uses this
# target so its portable engine configuration is not reconfigured here.
bundle:
	rm -rf "$(APP_DIR)"
	mkdir -p "$(MACOS)" "$(BIN)"
	DEVELOPER_DIR="$(DEVELOPER_DIR)" CLANG_MODULE_CACHE_PATH="$(BUILD_DIR)/ModuleCache" xcrun swiftc -O -parse-as-library \
		-sdk "$$(DEVELOPER_DIR="$(DEVELOPER_DIR)" xcrun --sdk macosx --show-sdk-path)" \
		-target "$(SWIFT_TARGET)" \
		-Xcc -fmodules-cache-path="$(BUILD_DIR)/ModuleCache" \
		-framework AppKit -framework SwiftUI -framework UserNotifications \
		-o "$(MACOS)/$(APP_NAME)" \
		Sources/Bckbrnr/*.swift
	cp Info.plist "$(CONTENTS)/Info.plist"
	cp ../../LICENSE "$(RESOURCES)/LICENSE"
	plutil -lint "$(CONTENTS)/Info.plist"
	install -m 755 "$(ENGINE_DIR)/nrvnad" "$(BIN)/nrvnad"
	install -m 755 "$(ENGINE_DIR)/wrk" "$(BIN)/wrk"
	install -m 755 "$(ENGINE_DIR)/flw" "$(BIN)/flw"
	# Ad-hoc sign, nested binaries first. An unsigned bundle cannot deliver
	# UserNotifications: authorization fails silently and no permission
	# prompt ever appears. Ad-hoc is not Developer ID signing; Gatekeeper
	# still applies to downloaded archives.
	codesign --force -s - "$(BIN)/nrvnad" "$(BIN)/wrk" "$(BIN)/flw"
	codesign --force -s - "$(APP_DIR)"

run: app
	open "$(APP_DIR)"

test:
	@mkdir -p "$(BUILD_DIR)/ModuleCache"
	DEVELOPER_DIR="$(DEVELOPER_DIR)" xcrun swiftc -parse-as-library \
		-Xcc -fmodules-cache-path="$(BUILD_DIR)/ModuleCache" \
		-o "$(BUILD_DIR)/naming-tests" \
		Sources/Bckbrnr/Naming.swift Tests/NamingTests.swift
	"$(BUILD_DIR)/naming-tests"

integration-test:
	@test -n "$(MODEL)" || { echo "MODEL=/path/to/model.gguf is required" >&2; exit 2; }
	@mkdir -p "$(BUILD_DIR)/ModuleCache"
	DEVELOPER_DIR="$(DEVELOPER_DIR)" xcrun swiftc -O \
		-Xcc -fmodules-cache-path="$(BUILD_DIR)/ModuleCache" \
		-framework AppKit -framework UserNotifications \
		-o "$(BUILD_DIR)/controller-journey-tests" \
		Sources/Bckbrnr/Controller.swift Sources/Bckbrnr/EnginePaths.swift \
		Sources/Bckbrnr/Naming.swift Tests/ControllerJourneyTests.swift
	BCKBRNR_ENGINE_DIR="$(ENGINE_DIR)" BCKBRNR_TEXT_MODEL="$(MODEL)" \
		"$(BUILD_DIR)/controller-journey-tests"

clean:
	rm -rf "$(BUILD_DIR)"
