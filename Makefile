.PHONY: docker-shell docker-build docker-flash clean

# Local dev uses the same prebaked GHCR image as CI.
# Override IMAGE if you need a pinned tag.
IMAGE ?= ghcr.io/kdmukai-bot/2048-esp32-base:latest

docker-shell:
	docker run --rm -it \
		--user $(shell id -u):$(shell id -g) \
		-e HOME=/tmp/home \
		-e XDG_CACHE_HOME=/tmp/home/.cache \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(PWD)/.ccache:/tmp/home/.cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash

# Build firmware inside Docker
docker-build:
	docker run --rm -t \
		--user $(shell id -u):$(shell id -g) \
		-e HOME=/tmp/home \
		-e XDG_CACHE_HOME=/tmp/home/.cache \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(PWD)/.ccache:/tmp/home/.cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -lc './scripts/docker_build.sh'

# Flash via USB (requires device access — run outside Docker or with --device)
docker-flash:
	docker run --rm -it \
		--user $(shell id -u):$(shell id -g) \
		-e HOME=/tmp/home \
		-e XDG_CACHE_HOME=/tmp/home/.cache \
		--device /dev/ttyACM0 \
		-v $(PWD):/workspace/2048-esp32 \
		-v $(PWD)/.ccache:/tmp/home/.cache \
		-w /workspace/2048-esp32 \
		$(IMAGE) bash -lc './scripts/docker_flash.sh'

# Safe clean: remove build outputs
clean:
	rm -rf build sdkconfig sdkconfig.old managed_components dependencies.lock

# Deeper clean: clean + purge local ccache
clean-purge-ccache: clean
	rm -rf .ccache
