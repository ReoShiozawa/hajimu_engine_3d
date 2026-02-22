PLUGIN_NAME   = engine_3d
BUILD_DIR     = build
VENDOR_DIR    = vendor
INSTALL_DIR   = $(HOME)/.hajimu/plugins/$(PLUGIN_NAME)
OUTPUT        = $(BUILD_DIR)/$(PLUGIN_NAME).hjp
STB_IMAGE     = $(VENDOR_DIR)/stb_image.h

NCPU          = $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: all vendor clean install uninstall

all: vendor $(OUTPUT)

$(OUTPUT): $(BUILD_DIR)/Makefile
	cmake --build $(BUILD_DIR) -j$(NCPU)

$(BUILD_DIR)/Makefile: CMakeLists.txt
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -Wno-dev

vendor: $(STB_IMAGE)

$(STB_IMAGE):
	@mkdir -p $(VENDOR_DIR)
	curl -fsSL -o $@ \
		https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
	@echo "  ダウンロード完了: $@"

install: all
	@mkdir -p $(INSTALL_DIR)
	cp $(OUTPUT) $(INSTALL_DIR)/
	@echo "  インストール: $(INSTALL_DIR)/$(PLUGIN_NAME).hjp"

uninstall:
	rm -f $(INSTALL_DIR)/$(PLUGIN_NAME).hjp

clean:
	rm -rf $(BUILD_DIR)
