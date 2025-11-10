# 7.5.0.0
Beta release built with Gecko SDK 4.4.6.

# 7.4.4.6
For adapters that support RGB LEDs, fix the color XNCP command parsing.

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
