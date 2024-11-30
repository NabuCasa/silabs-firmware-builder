TOPDIR = $(shell pwd)
DOCKER_CHECK := $(shell command -v docker 2> /dev/null)
PODMAN_CHECK := $(shell command -v podman 2> /dev/null)

ifdef PODMAN_CHECK
    CONTAINER_ENGINE ?= podman
else ifdef DOCKER_CHECK
    CONTAINER_ENGINE ?= docker
endif

CONTAINER_NAME ?= silabs-firmware-builder
CONTAINER_USER_GROUP ?= $(shell id -u):$(shell id -g)

ifeq ($(CONTAINER_ENGINE),docker)
    VOLUME_OPTS=
else
    VOLUME_OPTS=:z
endif

define run_in_container
	$(CONTAINER_ENGINE) \
		run --rm -it \
		--user $(CONTAINER_USER_GROUP) \
		-v $(TOPDIR):/build$(VOLUME_OPTS) \
		-v $(TOPDIR)/outputs:/outputs$(VOLUME_OPTS) \
		-v $(TOPDIR)/build_dir:/build_dir$(VOLUME_OPTS) \
		$(CONTAINER_NAME)
endef

MANIFESTS ?= $(shell find manifests -type f \( -name "*.yaml" -o -name "*.yml" \) -print)

all: build_container build_firmware

help:
	@echo "Usage: make [all|build_container|build_firmware]"
	@echo ""
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
ifneq ($(CONTAINER_ENGINE),docker)
	$(CONTAINER_ENGINE) unshare chown -R $(shell id -u):$(shell id -g) $@
endif

build_container:
	$(CONTAINER_ENGINE) build -t $(CONTAINER_NAME) .

build_firmware: ./outputs ./build_dir
	$(run_in_container) \
		bash -c " \
			build_firmware.sh \
			--build-dir /build_dir \
			--output-dir /outputs \
			$(foreach manifest,$(MANIFESTS),--manifest $(manifest)) \
		"
