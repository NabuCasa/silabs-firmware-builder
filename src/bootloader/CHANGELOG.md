# 3.2.2
ZWA-2 only: added a storage slot at 0x08044000–0x08074000, so the controller firmware can receive updates while it keeps running.

- The bootloader installs a valid file from the slot when it is entered. An interrupted install runs again on the next entry.
- The file is erased once it is installed.
- Leaving the XMODEM menu with "run" no longer counts towards the SDK's reset-loop stop. Without a valid application, the bootloader would otherwise stop responding on the third entry until it is power cycled.
- Applications must end below 0x08044000.

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
