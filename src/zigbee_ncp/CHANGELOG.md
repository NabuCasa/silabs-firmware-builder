# 9.1.1.0
Built with EmberZNet 9.1 from Simplicity SDK 2026.6.1, migrating us from Gecko SDK to Simplicity SDK. Faster message sending, Zigbee 4.0 support, and a few increased buffers.

- Built with EmberZNet 9.1 from Simplicity SDK 2026.6.1.
- Added Zigbee 4.0 support. This is fully backwards compatible with Zigbee 3.0 and 1.2.
- Sending a message now takes a single request instead of up to three with a new XNCP API. This reduces latency when sending requests with network coordinators.
- Increased network table sizes. On ZBT-2 the route and source route tables hold 254 entries, the address table 128, and up to 128 messages can be in flight at once, with a larger packet buffer heap. On SkyConnect and Yellow, 64 messages can be in flight and the address table holds 32 entries.
- Increased the serial receive buffer from 128 to 512 bytes.

# 9.1.0.0
Built with EmberZNet 9.1 from Simplicity SDK 2026.6.0, migrating us from Gecko SDK to Simplicity SDK.

# 9.0.2.0
Built with EmberZNet 9.0 from Simplicity SDK 2025.12.3.

# 7.5.1.0
Built with Gecko SDK 4.5.0. This includes a new feature to restore routes on adapter startup, speeding up network responsiveness after a reset.

# 7.5.0.0
Built with Gecko SDK 4.4.6.

# 7.4.4.6
For adapters that support RGB LEDs, fix the color XNCP command parsing. Allow the adapter to signal preferred TX power settings for a given regulatory domain.

# 7.4.4.5
Increase UART RX buffer size from 32 to 128 bytes to fix issues with OTA when using Z2M. For adapters with pinhole reset buttons, enable the reset button to wipe network settings. This is a bugfix re-release of 7.4.4.4.

# 7.4.4.4
Increase UART RX buffer size from 32 to 128 bytes to fix issues with OTA when using Z2M. For adapters with pinhole reset buttons, enable the reset button to wipe network settings.

# 7.4.4.3
Fix setup LED behavior for adapters with external indicator LEDs and tweak routing and child table sizes.

# 7.4.4.2
Allow receiving events from devices using unregistered group IDs and increase routing and child table sizes.

# 7.4.4.1
Re-release of 7.4.4.0 that fixes a regression with group addressing present in the previous release, affecting some devices like IKEA remotes.

# 7.4.4.0
Initial release with the new firmware builder. This release fixes minor bugs related to adapter migration and includes other improvements in the underlying SDK.
