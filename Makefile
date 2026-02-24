# =============================================================================
# engine_3d — はじむ言語用プラグイン
# クロスプラットフォーム Makefile (macOS / Linux / Windows MinGW)
# =============================================================================

PLUGIN_NAME = engine_3d
BUILD_DIR   = build
OUTPUT      = $(BUILD_DIR)/$(PLUGIN_NAME).hjp

# OS 判定 ($(OS) は Windows CMD/PowerShell で "Windows_NT" になる)
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    INSTALL_DIR := $(USERPROFILE)/.hajimu/plugins
    NCPU        := $(NUMBER_OF_PROCESSORS)
else
    DETECTED_OS := $(shell uname -s 2>/dev/null || echo Unknown)
    INSTALL_DIR := $(HOME)/.hajimu/plugins
    NCPU        := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
endif

CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Release -Wno-dev
VENDOR_DIR = vendor
STB_IMAGE  = $(VENDOR_DIR)/stb_image.h

vendor: $(STB_IMAGE)

$(STB_IMAGE):
	@mkdir -p $(VENDOR_DIR)
	curl -fsSL -o $@ https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
	@echo "  ダウンロード完了: $@"
.PHONY: all vendor clean install uninstall

all: vendor $(OUTPUT)

$(OUTPUT): CMakeLists.txt
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) -j$(NCPU)
	@echo "  ビルド完了: $(OUTPUT)"
clean:
ifeq ($(OS),Windows_NT)
	-rmdir /S /Q $(BUILD_DIR) 2>NUL
else
	rm -rf $(BUILD_DIR)
endif
	@echo "  クリーン完了"
install: all
ifeq ($(OS),Windows_NT)
	if not exist "$(INSTALL_DIR)\$(PLUGIN_NAME)" mkdir "$(INSTALL_DIR)\$(PLUGIN_NAME)"
	copy /Y $(OUTPUT) "$(INSTALL_DIR)\$(PLUGIN_NAME)"
else
	@mkdir -p $(INSTALL_DIR)/$(PLUGIN_NAME)
	cp $(OUTPUT) $(INSTALL_DIR)/$(PLUGIN_NAME)/
endif
	@echo "  インストール完了: $(INSTALL_DIR)/$(PLUGIN_NAME)/"

uninstall:
ifeq ($(OS),Windows_NT)
	-rmdir /S /Q "$(INSTALL_DIR)\$(PLUGIN_NAME)" 2>NUL
else
	rm -rf $(INSTALL_DIR)/$(PLUGIN_NAME)
endif
	@echo "  アンインストール完了"
