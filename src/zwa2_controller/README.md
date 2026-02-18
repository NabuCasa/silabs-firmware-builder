# Z-Wave Controller firmware

## Firmware versioning scheme

The versioning for the firmware follows the format `A.BBC`, where:

- `A = 1` is the hardware/device revision - for now, unless we've reached more than 25 SDK versions.
- `BB` is a number (0-25) that represents the Z-Wave SDK version the firmware was built with
- `C` is the revision (0-9) of our modifications on top of the SDK

Leading zeroes are omitted in versions, so `1.001` is represented as `1.1`.

Examples:
- `1.0`: Z-Wave SDK version 7.23.1, revision 0
- `1.15`: Z-Wave SDK version 7.23.2, revision 5
- `1.254`: (some future SDK version `25`), revision 4

### SDK version mapping

| `BB` | Simplicity SDK version         | Z-Wave SDK version   |
| ---- | ------------------------------ | -------------------- |
| `0`  | [`2024.12.1`][sisdk-2024.12.1] | [`7.23.1`][zdk-7.23.1] |
| `1`  | [`2024.12.2`][sisdk-2024.12.2] | [`7.23.2`][zdk-7.23.2] |
| `2`  | [`2025.12.1`][sisdk-2025.12.1] | [`8.0.0`][zdk-8.0.0]   |

<!-- SDK links -->
[sisdk-2024.12.1]: https://github.com/SiliconLabs/simplicity_sdk/releases/tag/v2024.12.1-0
[sisdk-2024.12.2]: https://github.com/SiliconLabs/simplicity_sdk/releases/tag/v2024.12.2
[sisdk-2025.12.1]: https://docs.silabs.com/sisdk-release-notes/2025.12.1/sisdk-release-notes-overview/
[zdk-7.23.1]: https://www.silabs.com/documents/public/release-notes/SRN14930-7.23.1.0.pdf
[zdk-7.23.2]: https://www.silabs.com/documents/public/release-notes/SRN14930-7.23.2.0.pdf
[zdk-8.0.0]: https://docs.silabs.com/sisdk-release-notes/2025.12.1/sisdk-zwave-release-notes/
