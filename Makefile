.PHONY: docker-shell docker-build docker-flash clean desktop-build desktop-run desktop-clean

# Board selection — override with: make docker-build BOARD=waveshare_s3_lcd35
BOARD ?= waveshare_s3_lcd35b

# Local dev uses the same prebaked GHCR image as CI.
# Override IMAGE if you need a pinned tag.
IMAGE ?= ghcr.io/kdmukai-bot/2048-esp32-base:latest
CACHE_DIR ?= $(HOME)/.cache/2048-esp32-build

# Run as host user so build artifacts aren't root-owned.
# Cache uses a host bind mount (auto-created by mkdir) instead of a Docker
# named volume, which avoids root-ownership problems.
DOCKER_USER ?= --user $(shell id -u):$(shell id -g)

docker-shell:
	@mkdir -p $(CACHE_DIR)
	docker run --rm -it \
		$(DOCKER_USER) \
		-e HOME=/tmp/build-home \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(CACHE_DIR):/cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -c 'mkdir -p /tmp/build-home && exec bash'

# Build firmware inside Docker
docker-build:
	@mkdir -p $(CACHE_DIR)
	docker run --rm -t \
		$(DOCKER_USER) \
		-e HOME=/tmp/build-home \
		-e BOARD=$(BOARD) \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(CACHE_DIR):/cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -c './scripts/docker_build.sh'

# Flash via USB (requires device access — run outside Docker or with --device)
docker-flash:
	@mkdir -p $(CACHE_DIR)
	docker run --rm -it \
		$(DOCKER_USER) \
		-e HOME=/tmp/build-home \
		--device /dev/ttyACM0 \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(CACHE_DIR):/cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -c './scripts/docker_flash.sh'

# Safe clean: remove build outputs
clean:
	rm -rf build sdkconfig sdkconfig.old managed_components dependencies.lock

# Deeper clean: clean + purge build cache
clean-purge-cache: clean
	rm -rf $(CACHE_DIR)

# ── Desktop simulator (SDL2 + system LVGL) ──
desktop-build:
	cmake -S boards/desktop -B boards/desktop/build
	cmake --build boards/desktop/build

desktop-run: desktop-build
	boards/desktop/build/game_2048_desktop

desktop-clean:
	rm -rf boards/desktop/build
