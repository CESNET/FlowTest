ifeq ($(CMAKE),)
CMAKE := $(shell which cmake3)
endif

ifeq ($(CMAKE),)
CMAKE := cmake
endif

ifeq ($(CLANG_FORMAT),)
CLANG_FORMAT := clang-format
endif

ifeq ($(RUN_CLANG_TIDY),)
RUN_CLANG_TIDY := run-clang-tidy
endif

ifeq ($(CTEST),)
CTEST := ctest
endif

CLANG_TIDY_CONF="$(shell pwd)/.clang-tidy"
SOURCE_DIR = tools/
SOURCE_REGEX = '.*\.\(cpp\|hpp\|c\|h\)'

.PHONY: all
all: build/Makefile
	@$(MAKE) --no-print-directory -C build -j $(shell nproc)

%: build/Makefile
	@$(MAKE) --no-print-directory -C build -j $(shell nproc) $@

build/Makefile: | build
	@cd build && $(CMAKE) $(CMAKE_ARGS) ..

build:
	@mkdir -p $@

.PHONY: format
format:
	@find $(SOURCE_DIR) -type f -regex $(SOURCE_REGEX) -print0 | xargs -0 $(CLANG_FORMAT) --dry-run

.PHONY: format-fix
format-fix:
	@find $(SOURCE_DIR) -type f -regex $(SOURCE_REGEX) -print0 | xargs -0 $(CLANG_FORMAT) -i

.PHONY: tidy
tidy: all
	$(RUN_CLANG_TIDY) -p build -quiet -j $(shell nproc) -config-file=$(CLANG_TIDY_CONF) $(SOURCE_DIR)

.PHONY: tidy-fix
tidy-fix: all
	$(RUN_CLANG_TIDY) -p build -quiet -fix -j $(shell nproc) -config-file=$(CLANG_TIDY_CONF) $(SOURCE_DIR)

.PHONY: test
test: all
	$(CTEST) --test-dir build
