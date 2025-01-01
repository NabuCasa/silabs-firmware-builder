# Silicon Labs firmware builder repository

This repository contains Dockerfiles and GitHub actions that build Silicon Labs firmware. It also hosts unofficial Zigbee Coordinator and Thread (OpenThread) firmware builds that community members can experiment with at your own risk.

The firmware builder uses the Silicon Labs [Gecko SDK (GSDK)](https://github.com/SiliconLabs/gecko_sdk) and proprietary Silicon Labs tools such as the Silicon Labs Configurator (slc) and the Simplicity Commander standalone utility. This is a fork of the [Silabs firmware builder by Nabu Casa](https://github.com/NabuCasa/silabs-firmware-builder), adding support for additional radio adapter hardware models.

Again, please note that the pre-compiled firmware builds hosted in this repository are both unofficial and experimental or cutting-edge releases with  minimal testing which may brick your radio adapter so that it requires manual recovery via a compatible debug probe adapter, however, the builds offered via the Web Flasher are the latest versions recommended by the community.

## Supported hardware:
* Sonoff ZBDongle-E by ITead (based on EFR32MG21)
* Sonoff iHost by ITead (also based on EFR32MG21 and uses the same firmware as Sonoff ZBDongle-E)
* EasyIot ZB-GW04 v1.1 and v1.2 (based on EFR32MG21)
* Smlight SLZB-07 (which may require unlocked [bootloader](https://github.com/darkxst/silabs-firmware-builder/raw/main/firmware_builds/slzb-07/BTL_SLZB07.gbl) first) (based on EFR32MG21)
* Smlight SLZB-06M (based on EFR32MG21)
* Aeotec Zi-Stick (model “ZGA008” based on EFR32MG21)
* Sparkfun Things Matter MGM240P (requires [bootloader](https://github.com/darkxst/silabs-firmware-builder/blob/main/firmware_builds/mgm240p/bootloader-uart-xmodem_NCP.hex) to be flashed first using Silabs [Simplicity Commander](https://community.silabs.com/s/article/simplicity-commander?language=en_US)) (based on EFR32MG24)
* Elelabs Zigbee USB Adapter ELU013 (based on EFR32MG13P)
* Elelabs Zigbee Raspberry Pi Shield ELR023 (based on EFR32MG13P)

## Different firmware variants

Three network protocol application firmware variants are available:

* **EmberZNet NCP** = Zigbee NCP (Network Co-Processor) is used as a dedicated Zigbee Coordinator for Zigbee-only environments, for direct use with Zigbee2MQTT, Home Assistant's ZHA integration, other Zigpy based Zigbee Gateway implementations, or other Zigbee gateways/frameworks that support the EZSP (EmberZNet Serial Protocol) interface.
* **OpenThread RCP** firmware (experimental) = This [Thread](https://en.wikipedia.org/wiki/Thread_(network_protocol)) RCP (Radio Co-Processor) is used directly as a dedicated Thread Border Router in Thread-only environments, used for [OpenThread Border Router add-on](https://github.com/home-assistant/addons/blob/master/openthread_border_router/DOCS.md) or wpantund.
* **RCP Multi-PAN** (no longer recommended) = Multiprotocol firmware for concurrent communication over Zigbee and Thread via Home Assistant [SiliconLabs Multiprotocol add-on](https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md).

**Note!** Beware that the RCP MultiPAN in multiprotocol mode is no longer recommended because running multi-protocol with multiple active networks on a single radio adapter has proven to not be stable when using Zigbee and Thread network protocols simultaneously on the same radio adapter, it also increases the complexity of software component dependencies needed, so if already using RCP Multi-PAN then it is highly recommended that you plan to migrate to separate dedicated radio adapters instead, (using Zigbee NCP and Thread RCP firmware respectively), even if using RCP MultiPAN on a single radio adapter dongle has been working fine for you so far.

External reference explaining these different co-processor designs at a high level:
  * https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md
  * https://openthread.io/platforms/co-processor

## Web Flasher
Flash directly from your browser (only Chrome and Edge supported) [SL Web Flasher](https://darkxst.github.io/silabs-firmware-builder/)

Read this [blog post](https://dialedin.com.au/blog/sonoff-zbdongle-e-rcp-firmware) for more details and instructions for using RCP Multi-pan firmware.

## Home Assistant Add-on
**RCP MultiPan firmware ONLY**  
You can install this HA add-on to keep your dongle up to date with latest the RCP  Multi-Pan firmware. This is only for use if you are using the [SiliconLabs Multiprotocol add-on](https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md).

[Multipan Flasher Addon](https://github.com/darkxst/multipan_flasher/tree/main)

It can also be used for the initial conversion to MultiPan firmware, however you will need to disable ZHA or Zigbee2MQTT while you do this initial flash. You can then install and configure the Silabs Multiprotocol Add-on.

## Pre-Compiled Firmware
Firmware builds can be found in the [firmware_builds](https://github.com/darkxst/silabs-firmware-builder/tree/main/firmware_builds) folder.

`ncp-uart-hw-` EmberZnet pure Zigbee  
`rcp-uart-802154-` RCP MultiPan  
`ot-rcp-` OpenThread Only  

ZBDongle-E and ZB-GW04 v1.1 do not support hardware flow control. Yellow, SkyConnect and ZB-GW04 v1.2 are built with hardware flow control. Various baudrates are available as listed at the end of the filename.

Use NabuCasa's [Universal-Silabs-Flasher](https://github.com/NabuCasa/universal-silabs-flasher) to flash the `.gbl` files.


## Building locally

To build a firmware locally the build container can be reused. If you use VSCode then simply open the included devcontainer. Or you can manually start the
container locally using Docker, with a build directory bind-mounted, e.g.

```sh
docker run --rm -it \
  --user builder \
  -v $(pwd)/build:/build \
  ghcr.io/darkxst/silabs-firmware-builder:4.2.2
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

## Support

If you would like to help support further development of my `Silabs firmware` projects consider buying me a coffee!

<a href="https://www.buymeacoffee.com/darkxst" target="_blank"><img src="img/blue-button.png" alt="Buy Me A Coffee" height="41" width="174"></a>
