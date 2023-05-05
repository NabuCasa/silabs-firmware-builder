# Silicon Labs firmware builder repository

This repository contains Dockerfiles and GitHub actions which build Silicon Labs
firmware for Home Assistant Yellow and SkyConnect.

It uses the Silicon Labs Gecko SDK and proprietary Silicon Labs tools such as
the Silicon Labs Configurator (slc) and the Simplicity Commander standalone
utility.

## Building locally

To build a firmware locally the build container can be reused. Simply start the
container local with this repository bind-mounted as /build, e.g.

```sh
docker run --rm -it \
  --user builder \
  -v $(pwd):/build -v ~/.gitconfig:/home/builder/.gitconfig \
  ghcr.io/nabucasa/silabs-firmware-builder:4.2.3
```

To generate a project, use `slc generate`. To replicate/debug build issues in
an existing GitHub action, it is often helpful to just copy the command from
the "Generate Firmware Project" step. E.g. to build the Multiprotocol firmware
for Yellow:

```sh
  slc generate \
      --with="MGM210PA32JIA,simple_led:board_activity" \
      --project-file="/gecko_sdk/protocol/openthread/sample-apps/ot-ncp/rcp-uart-802154.slcp" \
      --export-destination=rcp-uart-802154-yellow \
      --copy-proj-sources --new-project --force \
      --configuration=""
```

To build the EmberZNet firmware for SkyConnect

```sh
  slc generate \
      --with="EFR32MG21A020F512IM32" \
      --project-file="/gecko_sdk/protocol/zigbee/app/ncp/sample-app/ncp-uart-hw/ncp-uart-hw.slcp" \
      --export-destination=ncp-uart-hw-skyconnect \
      --copy-proj-sources --new-project --force \
      --configuration="SL_IOSTREAM_USART_VCOM_RX_BUFFER_SIZE:64,EMBER_APS_UNICAST_MESSAGE_COUNT:20,EMBER_NEIGHBOR_TABLE_SIZE:26,EMBER_SOURCE_ROUTE_TABLE_SIZE:200,"
```

Apply patches to the generated firmware (Note: some firmwares also need patches
to be applied to the SDK, see GitHub Action files):

```sh
cd ncp-uart-hw-skyconnect
for patchfile in ../EmberZNet/SkyConnect/*.patch; do patch -p1 < $patchfile; done
```

Then build it using commands from the "Build Firmware" step:

```sh
make -f ncp-uart-hw.Makefile release
```

### Update patches

If you want to change patches, instead of applying them using patch, it is
easier to initialize a temporary git repo and apply them using git. Then ammend
commits or add new commits ontop, and regenerate the patches:

```sh
git init .
git add .
git commit -m "initial commit"
git am ../EmberZNet/SkyConnect/*.patch 
<make change>
git format-patch -N --output-directory=../EmberZNet/SkyConnect/ HEAD~2
```

