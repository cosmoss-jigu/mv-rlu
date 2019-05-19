CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

include $(CUR_DIR)/../Makefile.inc

DIRS = $(wildcard */)

all: kyotocabinet
	@for d in $(DIRS); \
                do ( echo; \
                        echo '==================================================='; \
                        echo '[*] BUILDING '$$d; \
                        (cd $$d && $(MAKE) -j$(NJOB)); \
                ); \
        done

kyotocabinet:
	(cd $(CUR_DIR)/kyotocabinet && $(MAKE) clean && VANILLA=1 $(MAKE) -j$(NJOB))
	(cd $(CUR_DIR)/kyotocabinet && $(MAKE) clean && RLU=1 $(MAKE) -j$(NJOB))
	(cd $(CUR_DIR)/kyotocabinet && $(MAKE) clean && MVRLU=1 $(MAKE) -j$(NJOB))
	(cd $(CUR_DIR)/kyotocabinet && $(MAKE) install-all)

clean:
	@for d in $(DIRS); \
                do ( echo; \
                        echo '==================================================='; \
                        echo '[*] CLEAN '$$d; \
                        (cd $$d && $(MAKE) clean); \
                ); \
        done

distclean: clean
	(cd $(CUR_DIR)/kyotocabinet && $(MAKE) distclean)

.PHONY: all clean distclean kyotocabinet

