CURRENT_DIR:=$(shell pwd)

BUILD_DIR?=${CURRENT_DIR}/build
BUILD_TYPE?=Release
INSTALL_DIR?=${CURRENT_DIR}/install
# ON or OFF
RUNTIME_LTO?=ON
ifndef NOELLE_INSTALL_DIR
$(error NOELLE_INSTALL_DIR is not set)
endif
ifndef SCAF_INSTALL_DIR
$(error SCAF_INSTALL_DIR is not set)
 endif

all: install

.PHONY: build install clean check-path

check-path:
	@echo "Building in ${BUILD_DIR}"
	@echo "Installing in ${INSTALL_DIR}"
	@echo "Build type: ${BUILD_TYPE}"
	@echo "Runtime LTO: ${RUNTIME_LTO}"

build: check-path
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) -DRUNTIME_LTO=$(RUNTIME_LTO) ../
	cd $(BUILD_DIR) && make -j4

install: build
	cd $(BUILD_DIR) && make install
	cp -r scripts/* $(INSTALL_DIR)/bin
	@echo "export RUNTIME_LTO=$(RUNTIME_LTO)" > $(INSTALL_DIR)/PROMPT.rc
	@echo "export SLAMP_INSTALL_DIR=$(INSTALL_DIR)" >> $(INSTALL_DIR)/PROMPT.rc
	@echo "export PATH=$(INSTALL_DIR)/bin:\$$PATH" >> $(INSTALL_DIR)/PROMPT.rc
	@echo "export NOELLE_LIBS_DIR=$(NOELLE_INSTALL_DIR)/lib" >> $(INSTALL_DIR)/PROMPT.rc
	@echo "export SCAF_LIBS_DIR=$(SCAF_INSTALL_DIR)/lib" >> $(INSTALL_DIR)/PROMPT.rc


clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(INSTALL_DIR)
