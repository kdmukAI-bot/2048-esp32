.PHONY: docker-shell docker-build docker-flash dist clean desktop-build desktop-run desktop-clean

# Board selection: override with make docker-build BOARD=waveshare_s3_lcd35
BOARD ?= waveshare_s3_lcd35b
# BOARD_VARIANT selects a board variant. ESP32-P4 boards build for chip revision v3.x by
# default; BOARD_VARIANT=PRE_REV3 builds for the older v0.x/v1.x silicon. A variant
# packages into dist/<BOARD>-<VARIANT>/, so the two images never overwrite each other.
BOARD_VARIANT ?=
BUILD_NAME = $(BOARD)$(if $(BOARD_VARIANT),-$(BOARD_VARIANT))
DIST_DIR = dist/$(BUILD_NAME)

# Local dev uses the same prebaked GHCR image as CI, pinned to the ESP-IDF baseline tag
# so it moves in step with ESP_IDF_REF in Dockerfile.ghcr. Override IMAGE to use
# another tag.
IMAGE ?= ghcr.io/kdmukai-bot/2048-esp32-base:idf-v5.5.5
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
		-v $(CURDIR):/workspace/2048-esp32 \
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
		-e BOARD_VARIANT=$(BOARD_VARIANT) \
		-v $(CURDIR):/workspace/2048-esp32 \
		-v $(CACHE_DIR):/cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -c './scripts/docker_build.sh'

# Flash via USB (requires device access: run outside Docker or with --device)
docker-flash:
	@mkdir -p $(CACHE_DIR)
	docker run --rm -it \
		$(DOCKER_USER) \
		-e HOME=/tmp/build-home \
		--device /dev/ttyACM0 \
		-v $(CURDIR):/workspace/2048-esp32 \
		-v $(CACHE_DIR):/cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -c './scripts/docker_flash.sh'

# Collect flashable artifacts into dist/<BOARD>/ (dist/<BOARD>-<VARIANT>/ for a variant)
dist: docker-build
	rm -rf $(DIST_DIR)
	mkdir -p $(DIST_DIR)/bootloader $(DIST_DIR)/partition_table
	cp build/flash_args $(DIST_DIR)/
	cp build/game_2048.bin $(DIST_DIR)/
	cp build/bootloader/bootloader.bin $(DIST_DIR)/bootloader/
	cp build/partition_table/partition-table.bin $(DIST_DIR)/partition_table/
	@if [ -f build/chip_revisions.txt ]; then cp build/chip_revisions.txt $(DIST_DIR)/; fi

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
