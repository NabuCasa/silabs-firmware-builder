# 1.4.2
* Prevent a full NVM restore from erasing the flash page after the NVM which contains the bootloader keys. If this update is aborted right at the start (the Z-Wave JS log shows `file error 0x44`), the bootloader keys were erased already. Restore them with the [ZWA-2 toolbox](https://home-assistant.github.io/zwa2-toolbox/), then update again.

# 1.4.1
* Fixed the top LED not responding to LED commands, and staying stuck at a dim brightness, since 1.3.0. The Z-Wave UART driver reset the DMA controller at startup, which stopped the LED driver's transfers from completing on Simplicity SDK 2026.6.
* Fixed the LED state being reported as off after a reboot, even when it was restored as on.

# 1.4.0
* Updated to Simplicity SDK 2026.6.2 (Z-Wave SDK 8.1.2), up from Simplicity SDK 2026.6.1 (Z-Wave SDK 8.1.1).
* Fixed an issue where the controller could stop sending frames after several transmission failures on a congested network.
* Fixed the Serial API Setup command Set Max LR Tx Power, which did not apply the requested Long Range transmit power on 20 dBm devices like the ZWA-2.
* Fixed an issue where NVM backup and restore could read or write outside the NVM region.
* Prevented an external SUC Node ID advertisement from removing the SUC role from the controller.
* Added validation of NODES_EXIST topology updates.

# 1.3.1
* Added a proprietary Serial API command to query the bootloader version and capabilities without rebooting into the bootloader.
* Fixed an issue where the radio could turn off after one received frame. This affected some controllers migrated from the 500 series, where the listening flag in the controller's own node info was left unset.

# 1.3.0
Updated to Simplicity SDK 2026.6.1 (Z-Wave SDK 8.1.1), restoring runtime Image Rejection calibration and fixing a transmission stall under LBT/CSMA.

* Updated to Simplicity SDK 2026.6.1 (Z-Wave SDK 8.1.1), up from Simplicity SDK 2025.12.1 (Z-Wave SDK 8.0.0).
* Resynced the Serial API application with the new SDK reference application, including its DMA and sleep timer driver changes.
* Restored runtime Image Rejection calibration.
* Fixed a transmission stall that could occur under LBT/CSMA.
* Fixed fragmented-beam handling for the Japanese region.

# 1.2.0
* SDK updated to Simplicity SDK 2025.12.1.

# 1.1.0
* Correct default powerlevels for EU region
* Remove RGB and dimming functionality from the LED, convert to cold-white on/off light
* Change tilt indication to fast blinking

# 1.0.0
* Initial release based on Simplicity SDK 2024.12.1 and Z-Wave SDK 7.23.1

| Simplicity SDK                 | Z-Wave SDK             |
| ------------------------------ | ---------------------- |
| [`2024.12.1`][sisdk-2024.12.1] | [`7.23.1`][zdk-7.23.1] |

<!-- SDK links -->

[sisdk-2024.12.1]: https://github.com/SiliconLabs/simplicity_sdk/releases/tag/v2024.12.1-0
[zdk-7.23.1]: https://www.silabs.com/documents/public/release-notes/SRN14930-7.23.1.0.pdf

