PROJ_DIR  := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
GIT_DIR   := $(PROJ_DIR)/.git
TOOLS_DIR := $(PROJ_DIR)/tools
INC_DIR   := $(PROJ_DIR)/include
LIB_DIR   := $(PROJ_DIR)/lib
BIN_DIR   := $(PROJ_DIR)/bin

AR = ar
CC = gcc
CXX = g++
MAKE = make

# -jN
ifeq ($(strip $(NJOB)), )
  NJOB := ${shell nproc}
endif

# malloc
ifeq ($(strip $(MEMMGR)), )
  MEMMGR += -ljemalloc
endif
