# 3.2.2
Added a storage slot to the ZWA-2 bootloader, so the ZWA-2 controller firmware can receive updates while it keeps running.

- ZWA-2: the slot spans 0x08044000–0x08074000. The bootloader installs a valid file from it when it is entered, and an interrupted install runs again on the next entry.
- ZWA-2: applications must end below 0x08044000.

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
