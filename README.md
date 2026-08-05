# Silicon Labs firmware builder repository
This repository contains tools for building Zigbee, Thread, and Z-Wave firmwares for the
Home Assistant Connect ZBT-1/SkyConnect, ZBT-2, ZWA-2, and Yellow. The firmware
manifests are entirely generic, however, and are intended to be written easily for any
Silicon Labs chips.

It uses the Silicon Labs Gecko SDK and proprietary Silicon Labs tools such as the
Silicon Labs Configurator (slc) and the Simplicity Commander standalone utility.

## Background
The project templates in this repository are configured and built for specific boards
using manifest files. For example, the [`zbt2_zigbee_ncp.yaml`](https://github.com/NabuCasa/silabs-firmware-builder/blob/main/manifests/nabucasa/zbt2/zbt2_zigbee_ncp.yaml)
manifest file configures the Zigbee firmware for the Connect ZBT-2.

# Building firmwares
The easiest way to build firmware is using Docker. The container image includes all
required SDKs, toolchains, and tools pre-installed.

```bash
git clone https://github.com/NabuCasa/silabs-firmware-builder
cd silabs-firmware-builder

docker run --rm -v $(pwd):/repo ghcr.io/nabucasa/silabs-firmware-builder \
    --manifest manifests/nabucasa/skyconnect_zigbee_ncp.yaml \
    --output gbl \
    --output-dir artifacts
```

Once the build is complete, the firmware will be in the `artifacts` directory.

# Development
## Setting up Simplicity Studio (for development)
If you are going to be developing using Simplicity Studio, note that each project can
potentially use a different Gecko SDK release. It is recommended to forego the typical
Simplicity Studio SDK management workflow and manually manage SDKs:

1. Clone a specific version of the Gecko SDK:
   ```bash
   # For macOS
   mkdir ~/SimplicityStudio/SDKs/gecko_sdk_4.4.2
   cd ~/SimplicityStudio/SDKs/gecko_sdk_4.4.2

   git clone -b v4.4.2 https://github.com/SiliconLabs/gecko_sdk .
   git checkout -b branch_tag
   ```

2. Open preferences, navigate to **Simplicity Studio > SDKs**, click the `Add SDK...` button, and browse to the above location.
3. Once the SDK is added, select its entry and click `Add Extension...`.
4. In this repo, add the extensions under `gecko_sdk_extensions`.

Repeat this process for every necessary SDK version.

> [!TIP]
> If you have build issues after switching commits, make sure to delete any
> `gecko_sdk_*` and `template` folders from the Simplicity working tree.
