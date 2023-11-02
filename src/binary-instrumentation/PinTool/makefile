# Test for PIN_ROOT is set

ifndef PIN_ROOT
$(error PIN_ROOT is not set)
endif

CC := gcc
CXX := g++

TOOL_ROOTS := count_mem buffer_linux
all: obj-intel64/count_mem.so obj-intel64/buffer_linux.so

CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
include $(CONFIG_ROOT)/makefile.config
include $(CONFIG_ROOT)/makefile.default.rules

