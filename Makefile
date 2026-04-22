.PHONY: build test test-qml test-tray test-all run install uninstall release clean help setup-hooks package-deb package-rpm package-arch diagrams diagrams-check

# All d2 sources under docs/wiki/diagrams/ and their matching SVGs.
D2_SRCS := $(wildcard docs/wiki/diagrams/*.d2)
D2_SVGS := $(D2_SRCS:.d2=.svg)

IS_CONTAINER := $(shell test -f /.dockerenv && echo 1)

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "} !seen[$$1]++ {printf "\033[36m%-15s\033[0m %s\n", $$1, $$2}' | sort

build: ## Build the project
	@cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev
	@cmake --build build -j$$(nproc)

test: ## Run C++ unit/integration tests
	@QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tests

test-qml: ## Run QML component tests
	@QT_QPA_PLATFORM=offscreen ./build/tests/qml/logitune-qml-tests

test-tray: ## Run tray manager tests
	@QT_QPA_PLATFORM=offscreen ./build/tests/logitune-tray-tests

test-all: test test-tray test-qml ## Run all tests

diagrams: $(D2_SVGS) ## Regenerate SVG diagrams from d2 sources

docs/wiki/diagrams/%.svg: docs/wiki/diagrams/%.d2
	@command -v d2 >/dev/null 2>&1 || { echo "error: d2 not installed (pacman -S d2 / https://d2lang.com)"; exit 1; }
	@d2 --theme 200 --sketch=false --pad 40 "$<" "$@"
	@echo "regenerated $@"

diagrams-check: ## Fail if any SVG is stale against its .d2 source
	@for src in $(D2_SRCS); do \
	    svg="$${src%.d2}.svg"; \
	    tmp=$$(mktemp --suffix=.svg); \
	    d2 --theme 200 --sketch=false --pad 40 "$$src" "$$tmp" >/dev/null 2>&1 || { \
	        echo "error: d2 failed on $$src"; rm -f "$$tmp"; exit 1; }; \
	    if ! diff -q "$$svg" "$$tmp" >/dev/null 2>&1; then \
	        echo "error: $$svg is out of sync with $$src"; \
	        echo "       run: make diagrams"; \
	        rm -f "$$tmp"; exit 1; \
	    fi; \
	    rm -f "$$tmp"; \
	done
	@echo "all d2 SVGs up to date."

ifdef IS_CONTAINER
setup-hooks: ## (redundant) cmake configure now sets core.hooksPath=hooks automatically
	@echo "setup-hooks: nothing to do — 'cmake -B build' already activates hooks/ via core.hooksPath."
	@echo "See hooks/pre-push for the active pre-push script."
else
run: build ## Build and run the app (host only)
	@./build/src/app/logitune --debug

install: ## Install to system (host only)
	@cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -Wno-dev
	@cmake --build build -j$$(nproc)
	@sudo cmake --install build
	@sudo udevadm control --reload-rules
	@sudo udevadm trigger
	@echo "✅ Installed. Run: logitune"

uninstall: ## Uninstall from system (host only)
	@sudo rm -f /usr/bin/logitune
	@sudo rm -f /usr/lib/udev/rules.d/71-logitune.rules
	@sudo rm -f /usr/share/applications/logitune.desktop
	@sudo rm -f /usr/etc/xdg/autostart/logitune.desktop
	@sudo rm -f /usr/share/icons/hicolor/scalable/apps/com.logitune.Logitune.svg
	@sudo rm -f /usr/share/metainfo/com.logitune.Logitune.metainfo.xml
	@sudo rm -rf /usr/share/gnome-shell/extensions/logitune-focus@logitune.com
	@sudo udevadm control --reload-rules
	@echo "✅ Uninstalled"

package-deb: ## Build .deb package (Debian/Ubuntu)
	@scripts/package-deb.sh

package-rpm: ## Build .rpm package (Fedora/openSUSE)
	@scripts/package-rpm.sh

package-arch: ## Build Arch/AUR PKGBUILD
	@scripts/package-arch.sh

release: ## Release — version bump, tag, push (host only)
	@./scripts/release.sh $(or $(BUMP),patch)

setup-hooks: ## (redundant) cmake configure now sets core.hooksPath=hooks automatically
	@echo "setup-hooks: nothing to do — 'cmake -B build' already activates hooks/ via core.hooksPath."
	@echo "See hooks/pre-push for the active pre-push script."
endif

clean: ## Remove build artifacts
	@rm -rf build
