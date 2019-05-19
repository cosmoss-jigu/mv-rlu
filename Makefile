CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

include $(CUR_DIR)/Makefile.inc

PHONY := all
all: lib benchmark

PHONY += clean
clean:
	(cd $(PROJ_DIR)/lib && make -s clean && \
	 cd $(PROJ_DIR)/benchmark && make -s clean)

PHONY += distclean
distclean: clean
	@echo -e "\033[0;32m# Clean everything completely...\033[0m"
	(cd $(PROJ_DIR)/benchmark && make distclean)
	rm -f $(BIN_DIR)/*-rlu
	rm -f $(BIN_DIR)/*-mvrlu
	rm -f $(BIN_DIR)/*-mvrlu-ordo
	rm -f $(BIN_DIR)/*-mvrlu-gclk
	rm -f $(BIN_DIR)/*-vanilla

PHONY += format
format: git-hooks
	@echo -e "\033[0;32m# Running clang-format...\033[0m"
	@clang-format -i $(INC_DIR)/*.[ch] $(LIB_DIR)/*.[ch]

git-hooks: $(GIT_DIR)/hooks/pre-commit

$(GIT_DIR)/hooks/pre-commit:
	@echo -e "\033[0;32m# Installing git pre-commit hook for formatting\033[0m"
	@ln -s $(TOOLS_DIR)/pre-commit $(GIT_DIR)/hooks/pre-commit

PHONY += benchmark
benchmark: lib
	(cd $(PROJ_DIR)/benchmark && make)

PHONY += benchmark-clean
benchmark-clean:
	(cd $(PROJ_DIR)/benchmark && make clean)

PHONY += lib
lib: git-hooks
	(cd $(PROJ_DIR)/lib && \
	 CONF=ordo make -j$(NJOB) && \
	 CONF=gclk make -j$(NJOB))

PHONY += lib-clean
lib-clean: git-hooks
	(cd $(PROJ_DIR)/lib && \
	 CONF=ordo make -j$(NJOB) clean && \
	 CONF=gclk make -j$(NJOB) clean)

PHONY += ordo
ordo:
	make -C tools/ordo/
	(cd tools/ordo && sudo ./gen_table.py)

PHONY += help
help: git-hooks
	@echo '## Generic targets:'
	@echo '  all             - Configure and build all source code including mv-rlu,'
	@echo '                    and kernel, and create a test VM image.'
	@echo '  clean           - Remove most generated files but keep the config'
	@echo '                    files, lib, and benchmarks except for kernel and vm.'
	@echo '                    To clean kernel, run `make kernel-clean`.'
	@echo '  distclean       - Remove all generated files and config files except'
	@echo '                    for the VM image. To remove a VM image, run `make'
	@echo '                    vm-clean`'
	@echo '  format          - Apply clang-format. Follow LLVM style for C++ code'
	@echo '                    and Linux kernel style for C code'
	@echo ''
	@echo '## Library targets:'
	@echo '  lib             - Build mv-rlu library'
	@echo '  lib-clean       - Clean mv-rlu library'
	@echo '  ordo            - Get ordo value of the server'
	@echo ''
	@echo '## Benchmark targets:'
	@echo '  benchmark       - Build all benchmarks'
	@echo '  benchmark-clean - Clean all benchmarks'

.PHONY: $(PHONY)

