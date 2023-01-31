ifeq ($(CMAKE),)
CMAKE := $(shell which cmake3)
endif

ifeq ($(CMAKE),)
CMAKE := cmake
endif

ifeq ($(CLANG_FORMAT),)
CLANG_FORMAT := clang-format
endif

SOURCE_DIR = tools/
SOURCE_REGEX = '.*\.\(cpp\|hpp\|c\|h\)'

.PHONY: all
all: build/Makefile
	@$(MAKE) --no-print-directory -C build

%: build/Makefile
	@$(MAKE) --no-print-directory -C build $@

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

