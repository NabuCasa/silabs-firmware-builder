# 1.3.1
* Added a proprietary Serial API command to query the bootloader version and capabilities without rebooting into the bootloader.

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

