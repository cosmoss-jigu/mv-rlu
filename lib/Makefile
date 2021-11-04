CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

include $(CUR_DIR)/../Makefile.inc

# configuration
ifeq ($(strip $(CONF)),gclk)
  LIB_SUFFIX = -gclk
endif

ifneq ($(strip $(CONF)),gclk) # default
  CFLAGS += -DMVRLU_ORDO_TIMESTAMPING
  LIB_SUFFIX = -ordo
endif

CFLAGS += -I$(INC_DIR)
CFLAGS += -march=native -mtune=native -O3
CFLAGS += -g
# CFLAGS += -Werror
CFLAGS += -Winline -Wall -Wextra -Wshadow -Wdeclaration-after-statement
CFLAGS += -Wno-unused-parameter -Wno-unused-value
CFLAGS += -Wno-unused-function -Wno-packed-not-aligned
# CFALGS += -DMVRLU_DISABLE_ADDR_ACTUAL_TYPE_CHECKING

LDFLAGS += -lpthread $(MEMMGR)

DEPS_DIR  := $(CUR_DIR)/.deps$(LIB_SUFFIX)
DEPCFLAGS = -MD -MF $(DEPS_DIR)/$*.d -MP

SRC_FILES = $(wildcard *.c)
OBJS_DIR  = $(CUR_DIR)/.objs$(LIB_SUFFIX)
OBJ_FILES = $(addprefix $(OBJS_DIR)/, $(SRC_FILES:.c=.o))

all: $(INC_DIR)/numa-config.h libmvrlu$(LIB_SUFFIX).a

$(OBJS_DIR)/%.o: %.c $(DEPS_DIR) $(OBJS_DIR)
	$(Q)$(CC) $(CFLAGS) $(DEPCFLAGS) -c -o $@ $<

$(OBJS_DIR):
	$(Q)mkdir -p $(OBJS_DIR)

$(DEPS_DIR):
	$(Q)mkdir -p $(DEPS_DIR)

$(INC_DIR)/numa-config.h:
	$(Q)$(TOOLS_DIR)/cpu-topology.py > $@

libmvrlu$(LIB_SUFFIX).a: $(OBJ_FILES)
	$(Q)/bin/rm -f $@
	$(Q)$(AR) cru $@ $^

clean:
	$(Q)rm -f libmvrlu*.a
	$(Q)rm -rf $(CUR_DIR)/.deps*
	$(Q)rm -rf $(CUR_DIR)/.objs*
	$(Q)rm -f $(INC_DIR)/numa-config.h

.PHONY: all clean
