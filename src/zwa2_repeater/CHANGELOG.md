# 1.3.0
* Updated to Simplicity SDK 2026.6.1 (Z-Wave SDK 8.1.1)
* The LEDs are now driven by the shared WS2812 driver from the nabucasa_hardware SDK extension, controlled by a timer instead of a dedicated task
* The default RF region is now EU
* Reconcile the persisted Z-Wave S2 identity with the manufacturing tokens on startup

# 1.2.0
* Match how the controller firmware adjusts region and powerlevels

# 1.1.0
* Added a CLI command to change the RF region

# 1.0.0
* Initial release based on Simplicity SDK 2024.12.1
