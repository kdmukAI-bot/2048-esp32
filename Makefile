.PHONY: docker-shell docker-build docker-flash clean desktop-build desktop-run desktop-clean

# Local dev uses the same prebaked GHCR image as CI.
# Override IMAGE if you need a pinned tag.
IMAGE ?= ghcr.io/kdmukai-bot/2048-esp32-base:latest
CACHE_VOL ?= 2048-esp32-cache

docker-shell:
	docker run --rm -it \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(CACHE_VOL):/root/.cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash

# Build firmware inside Docker
docker-build:
	docker run --rm -t \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(CACHE_VOL):/root/.cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -lc './scripts/docker_build.sh'

# Flash via USB (requires device access — run outside Docker or with --device)
docker-flash:
	docker run --rm -it \
		--device /dev/ttyACM0 \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(CACHE_VOL):/root/.cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -lc './scripts/docker_flash.sh'

# Safe clean: remove build outputs (may need sudo if created by root in container)
clean:
	rm -rf build sdkconfig sdkconfig.old managed_components dependencies.lock

# Deeper clean: clean + purge Docker cache volume
clean-purge-cache: clean
	docker volume rm $(CACHE_VOL) 2>/dev/null || true

# ── Desktop simulator (SDL2 + system LVGL) ──
desktop-build:
	cmake -S desktop -B desktop/build
	cmake --build desktop/build

desktop-run: desktop-build
	desktop/build/game_2048_desktop

desktop-clean:
	rm -rf desktop/build
