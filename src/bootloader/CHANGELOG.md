# 3.2.2
Uploads are staged in internal flash and only installed once they have arrived completely and verified.

- XMODEM uploads are written to a storage slot at 0x08044000–0x08074000. The running application stays intact until "run" is selected, and an interrupted install is retried on the next boot.
- Bootloader-only files are unpacked into the last 24 KB of the slot, so the application survives a bootloader update. Files with both a bootloader and an application replace the application.
- Files with Secure Element upgrades are refused.
- Applications must end below 0x08044000.
- Updating from 3.2.1 to 3.2.2 still deletes the application, because 3.2.1 writes uploads directly.

# 3.2.1
Updated to Simplicity SDK 2026.6.1, migrating the ZWA-2 bootloader from the standalone nc-zwave-bootloader repository.

- Updated to Simplicity SDK 2026.6.1, up from Simplicity SDK 2024.12.2.

# 3.2.0
Updated to Simplicity SDK 2026.6.1.

# 3.0.1
Initial ZWA-2 bootloader release based on Simplicity SDK 2024.12.2.

- Standalone UART XMODEM bootloader with enforced signed and encrypted upgrades.
- Added an "erase nvm" menu command that erases the Z-Wave NVM region while preserving the token page.
- The bootloader can be entered through the recessed button.
