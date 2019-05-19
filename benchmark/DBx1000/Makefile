CUR_DIR   := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
include $(CUR_DIR)/../../Makefile.inc

CC=g++
CFLAGS = -Wall -Wno-sign-compare -g -std=c++0x

.SUFFIXES: .o .cpp .h

SRC_DIRS = ./ ./benchmarks/ ./concurrency_control/ ./storage/ ./system/
INCLUDE = -I. -I./benchmarks -I./concurrency_control -I./storage -I./system -I$(INC_DIR)

CFLAGS += $(INCLUDE) -D NOGRAPHITE=1 -O1
LDFLAGS = -L. -L./libs -L$(LIB_DIR) -pthread -g -lrt -std=c++0x $(MEMMGR)

CPPS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)*.cpp))
OBJS = $(CPPS:.cpp=.o)
DEPS = $(CPPS:.cpp=.d)

all: rundb

rundb : $(OBJS) $(LIB_DIR)/libmvrlu-ordo.a $(INC_DIR)/numa-config.h
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(OBJS:%.o=%.d)

%.d: %.cpp
	$(CC) -MM -MT $*.o -MF $@ $(CFLAGS) $<

%.o: %.cpp
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f rundb $(OBJS) $(DEPS)

.PHONY: clean
