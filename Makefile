TOPDIR = $(shell pwd)
CONTAINER_TOOL = podman
CONTAINER_NAME = silabs-firmware-builder

ifeq ($(CONTAINER_TOOL),docker)
    VOLUME_OPTS=
else
    VOLUME_OPTS=:z
endif

define run_in_container
	$(CONTAINER_TOOL) \
		run --rm -it \
		--user $(shell id -u):$(shell id -g) \
		-v $(TOPDIR):/build$(VOLUME_OPTS) \
		-v $(TOPDIR)/outputs:/outputs$(VOLUME_OPTS) \
		-v $(TOPDIR)/build_dir:/build_dir$(VOLUME_OPTS) \
		$(CONTAINER_NAME)
endef

MANIFESTS ?= $(shell find manifests -type f \( -name "*.yaml" -o -name "*.yml" \) -print)

all: build_container build_firmware

help:
	@echo "Usage: make [all|build_container|build_firmware]"
	@echo "Targets:"
	@echo "  all             Build container and firmware"
	@echo "  build_container Build container"
	@echo "  build_firmware  Build firmware"
	@echo "  help            Show this help message"
	@echo ""
	@echo "Options:"
	@echo "  build_firmware MANIFESTS=<path>  Override default manifest files (default: all .yaml/.yml files in manifests/)"
	@echo ""
	@echo "Examples:"
	@echo "  # Build the container image"
	@echo "  make build_container"
	@echo ""
	@echo "  # Build all firmware manifests"
	@echo "  make build_firmware"
	@echo ""
	@echo "  # Build a specific firmware manifest"
	@echo "  make build_firmware MANIFESTS=manifests/nabucasa/yellow_bootloader.yaml"
	@echo ""

./outputs ./build_dir:
	mkdir -p $@
ifeq ($(CONTAINER_TOOL),docker)
	$(CONTAINER_TOOL) unshare chown -R $(shell id -u):$(shell id -g) $@
endif

build_container:
	$(CONTAINER_TOOL) build -t $(CONTAINER_NAME) .

build_firmware: ./outputs ./build_dir
	$(run_in_container) \
		bash -c " \
			cd /build && \
			./tools/build_firmware.sh \
			$(foreach manifest,$(MANIFESTS),--manifest $(manifest)) \
		"
