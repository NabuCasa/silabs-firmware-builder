# Home Assistant Connect ZWA-2 Z-Wave Zniffer

Z-Wave Zniffer firmware for the Home Assistant Connect ZWA-2. It turns the
stick into a passive Z-Wave traffic capture device that streams received frames
over the USB serial port, for use with the Silicon Labs Zniffer application or
the Z-Wave JS Zniffer.

The application sources are copied unmodified from the SDK's Z-Wave NCP Zniffer
sample application. The ZWA-2 specific changes are:

- The sample's `simple_led` LEDs are removed. The ZWA-2 has no plain GPIO LEDs.
- The Zniffer UART runs on USART0 (TX on PA08, RX on PA07).
- The DC-DC converter runs in bypass mode.

## Versioning

The firmware version is defined in the `zwa2_zniffer.yaml` manifest through
`USER_APP_VERSION`, `USER_APP_REVISION` and `USER_APP_PATCH`. It only ends up
in the GBL metadata. The Zniffer GetVersion command reports the ZAF version
(11.1 for Z-Wave SDK 8.1.2), because Z-Wave JS enables Long Range support only
for Zniffer versions 10.22 and newer.
