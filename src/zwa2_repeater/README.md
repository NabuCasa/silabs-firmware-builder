# Home Assistant Connect ZWA-2 Z-Wave Repeater

Z-Wave repeater firmware for the Home Assistant Connect ZWA-2. It turns the
stick into an always-on Z-Wave end device that repeats frames for the network.

The project is based on the Z-Wave LED Bulb sample application:

- Dimming support is removed. The on-board button toggles learn mode on a
  short press and factory-resets on a very long press.
- The RGB LEDs are driven by the shared WS2812 driver from the
  `nabucasa_hardware` SDK extension. The LED color can be controlled remotely
  through Color Switch CC, and the Indicator CC blinks the LEDs to identify
  the device.
- A CLI on the USB serial port allows changing the RF region and TX power,
  which are persisted in NVM (`set_region`, `set_powerlevel`, `bootloader`,
  ...).

## Versioning

The firmware version is defined in the `zwa2_repeater.yaml` manifest through
`USER_APP_VERSION`, `USER_APP_REVISION` and `USER_APP_PATCH`.
