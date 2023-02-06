CURRENT_DIR=$(pwd)
BUILD_DIR?=$CURRENT_DIR/build
BUILD_TYPE?=Release
INSTALL_DIR?=$CURRENT_DIR/install
# ON or OFF
RUNTIME_LTO?=OFF

all: install

.PHONY: build install clean

build:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) -DRUNTIME_LTO=$(RUNTIME_LTO) ../src
	cd $(BUILD_DIR) && make -j4

install: build
	cd $(BUILD_DIR) && make install
	# install scripts under install/bin
	cp -r scripts/* $(INSTALL_DIR)/bin
	# create .rc file
	echo "export PATH=$(INSTALL_DIR)/bin:\$$PATH" > $(INSTALL_DIR)/PROMPT.rc
	echo "export SLAMP_INSTALL_DIR=$(INSTALL_DIR)" >> $(INSTALL_DIR)/PROMPT.rc
	echo "export RUNTIME_LTO=$(RUNTIME_LTO)" >> $(INSTALL_DIR)/PROMPT.rc


clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(INSTALL_DIR)
