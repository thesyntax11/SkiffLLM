# Convenience wrapper around CMake/CTest for SkiffLLM.
# The authoritative build system is CMake; this file only adds friendly
# entry points for common workflows.

CMAKE ?= cmake
CTEST ?= ctest
JOBS  ?= 4

.PHONY: help all release debug sanitize tests check preset-release format install clean

help:
	@echo "SkiffLLM build targets:"
	@echo "  make release         Configure and build the Release preset"
	@echo "  make debug           Configure and build the Debug preset"
	@echo "  make sanitize        Build and run tests with ASan/UBSan"
	@echo "  make tests           Build Release and run CTest"
	@echo "  make check           Run the full local CI script"
	@echo "  make format          Check C++ formatting (requires clang-format)"
	@echo "  make install         Build and install to $$HOME/.local"
	@echo "  make clean           Remove generated build output"

all: release

release:
	$(CMAKE) --preset release
	$(CMAKE) --build build/release --config Release -j $(JOBS)

debug:
	$(CMAKE) --preset debug
	$(CMAKE) --build build/debug --config Debug -j $(JOBS)

preset-release: release

sanitize:
	$(CMAKE) --preset sanitize
	$(CMAKE) --build build/sanitize --config Debug -j $(JOBS)
	$(CTEST) --test-dir build/sanitize --output-on-failure

tests: release
	$(CTEST) --test-dir build/release --output-on-failure

check:
	bash scripts/ci-local.sh

format:
	bash scripts/check-format.sh

install: release
	bash scripts/install.sh

clean:
	rm -rf build
