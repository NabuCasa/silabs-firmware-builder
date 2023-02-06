# Silicon Labs firmware builder repository

This repository contains Dockerfiles and GitHub actions which build Silicon Labs
firmware for Home Assistant Yellow and SkyConnect.

It uses the Silicon Labs Gecko SDK and proprietary Silicon Labs tools such as
the Silicon Labs Configurator (slc) and the Simplicity Commander standalone
utility.

## Building locally

To build a firmware locally the build container can be reused. Simply start the
container local with a build directory bind-mounted, e.g.

```sh
docker run --rm -it \
  --user builder \
  -v $(pwd)/build:/build \
  ghcr.io/nabucasa/silabs-firmware-builder:4.1.4
```

To generate a project, use `slc generate`. To replicate/debug build issues in
an existing GitHub action, it is often helpful to just copy the command from
the "Generate Firmware Project" step.

```sh
  slc generate \
      --with="MGM210PA32JIA,simple_led:board_activity" \
      --project-file="/gecko_sdk/protocol/openthread/sample-apps/ot-ncp/rcp-uart-802154.slcp" \
      --export-destination=rcp-uart-802154-yellow \
      --copy-proj-sources --new-project --force \
      --configuration=""
```

Then build it using commands from the "Build Firmware" step:

```sh
cd rcp-uart-802154-yellow
make -f rcp-uart-802154.Makefile release
```
