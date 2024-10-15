# Silicon Labs firmware builder repository
This repository contains tools for building firmwares for the Home Assistant Connect
ZBT-1/SkyConnect and the Home Assistant Yellow's IEEE 802.15.4 radio. The firmware
manifests are entirely generic, however, and are intended to be written easily for any
Silicon Labs EFR32 device.

It uses the Silicon Labs Gecko/Simplicity SDK and proprietary Silicon Labs tools such as
the Silicon Labs Configurator (slc) and the Simplicity Commander standalone utility.

## Background
The project templates in this repository are configured and built for specific boards
using manifest files. For example, the [`skyconnect_zigbee_ncp.yaml`](https://github.com/NabuCasa/silabs-firmware-builder/blob/main/manifests/skyconnect_zigbee_ncp.yaml)
manifest file configures the Zigbee firmware for the SkyConnect/Connect ZBT-1.

## Setting up Simplicity Studio (for development)
If you are going to be developing using Simplicity Studio, note that each project can
potentially use a different Simplicity SDK release. It is recommended to forego the typical
Simplicity Studio SDK management workflow and manually manage SDKs:

1. Clone a specific version of the Simplicity SDK:
   ```bash
   # For macOS
   mkdir ~/SimplicityStudio/SDKs/simplicity_sdk_2024.6.2
   cd ~/SimplicityStudio/SDKs/simplicity_sdk_2024.6.2

   git clone -b v2024.6.2 https://github.com/SiliconLabs/simplicity_sdk .
   git checkout -b branch_tag
   ```

2. Open preferences, navigate to **Simplicity Studio > SDKs**, click the `Add SDK...` button, and browse to the above location.

Repeat this process for every necessary SDK version.

> [!TIP]
> If you have build issues after switching commits, make sure to delete any
> `simplicity_sdk_*`, `gecko_sdk_*`, and `template` folders from the project working tree.

## Building with a firmware manifest (for building device firmware)
Command line building requires:
- [`slc-cli`](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-tools-slc-cli/02-installation)
- [Simplicity Commander](https://www.silabs.com/developers/mcu-programming-options) (`commander`)
- The exact Simplicity SDK version required by the project. Note that this doesn't have to be a Git working tree, you can use [a GitHub release](https://github.com/SiliconLabs/gecko_sdk/releases).
- A compatible toolchain. Take a look at the `Dockerfile` for the necessary toolchains.

> [!TIP]
> If you have set up Simplicity Studio on macOS, everything will be automatically
> detected with the exception of `slc`. This is the only tool you need to download.

> [!WARNING]
> M1 users should set `JAVA_HOME=$(/usr/libexec/java_home -a x86_64)` when running the
> build command to make sure the correct Java version is picked by slc-cli. It currently
> is not compatible with ARM Java.

`slc-cli` maintains its own SDK and extension trust store so you first must trust all
SDK extensions for every SDK you plan to use:

```bash
slc signature trust --sdk ~/SimplicityStudio/SDKs/simplicity_sdk_2024.6.2
slc signature trust --sdk ~/SimplicityStudio/SDKs/simplicity_sdk_2024.6.2 --extension-path ~/SimplicityStudio/SDKs/simplicity_sdk_2024.6.2/extension/nc_efr32_watchdog_extension
```

`tools/build_project.py` is the main entry point for building firmwares. Provide paths
to potential SDKs and toolchains with the `--sdk` and `--toolchain` flags. The build
tool will automatically determine which SDK and toolchain to use.

> [!TIP]
> If you have set up Simplicity Studio on macOS, the default toolchain and SDK paths are
> automatically found so these flags can be omitted.

```bash
pip install ruamel.yaml  # Only dependency

python tools/build_project.py \
    # The following SDK and toolchain flags can be omitted on macOS, they are all autodetected
    --sdk ~/SimplicityStudio/SDKs/gecko_sdk_4.4.4 \
    --sdk ~/SimplicityStudio/SDKs/simplicity_sdk_2024.6.0 \
    --toolchain '/Applications/Simplicity Studio.app/Contents/Eclipse/developer/toolchains/gnu_arm/10.3_2021.10' \
    --toolchain '/Applications/Simplicity Studio.app/Contents/Eclipse/developer/toolchains/gnu_arm/12.2.rel1_2023.7' \

    --manifest manifests/skyconnect_ncp-uart-hw.yaml \
    --build-dir build \
    --output-dir output \

    # Generate `.gbl`, `.out`, and `.hex` firmwares
    --output gbl \
    --output out \
    --output hex
```

Once the build is complete, the firmwares will be in the `output` directory.
