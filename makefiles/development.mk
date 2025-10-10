# ==============================================================================
# Development Targets
#
# Description: This Makefile provides targets for development tasks such as
# linting, formatting, and testing. It is intended to be included in the main
# Makefile.
# ==============================================================================

.PHONY: setup build clean format check

# ==============================================================================
# Native Targets
# ==============================================================================

## Setup the build environment
setup:
	@printf "\n\033[1;33m⚙️  Setting up build environment\033[0m\n\n"
	@cmake -DLOG_LEVEL=$(LOG_LEVEL) -B$(BUILD_DIR)

## Build the project
build: setup
	@printf "\n\033[1;33m🔨 Building project\033[0m\n\n"
	@cmake --build $(BUILD_DIR)

## Clean build artifacts
clean:
	@printf "\n\033[1;33m🧹 Cleaning build artifacts\033[0m\n\n"
	@rm -rf $(BUILD_DIR) $(CCDB)

# ==============================================================================
# Code Quality Targets
# ==============================================================================

## Run clang-format and format all source files
format:
	@printf "\n\033[1;33m🎨 Formatting source files\033[0m\n\n"
	@files="$$(git ls-files '*.cpp' '*.h' '*.c' '*.hpp')"; \
	clang-format -i $$files; \
	printf "\033[1;32m✔ All files properly formatted\033[0m\n";

## Check code formatting using clang-format
check:
	@printf "\n\033[1;33m🔍 Checking formatting\033[0m\n\n"
	@files="$$(git ls-files '*.cpp' '*.h')"; \
	if clang-format --dry-run --Werror $$files >/dev/null 2>&1; then \
		printf "\033[1;32m✔ All files passed\033[0m\n"; \
	else \
		printf "\033[1;31m✘ Formatting issues detected\033[0m\n"; \
		clang-format --dry-run --Werror --output-replacements-xml  $$files; \
		exit 1; \
	fi

