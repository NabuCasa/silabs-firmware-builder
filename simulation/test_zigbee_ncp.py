import struct

import pytest

from simulation.device import (
    STANDARD_GRAVITY,
    XNCP_GET_ACCELEROMETER,
    XNCP_SET_LED_STATE,
    Rgb,
    xncp,
)

pytestmark = pytest.mark.fw_type("zigbee_ncp")


@pytest.mark.smoke
async def test_ezsp_startup(device):
    async with device.ezsp() as ezsp:
        assert ezsp.ezsp_version >= 13


@pytest.mark.board("zbt2")
async def test_led_color(device):
    async with device.ezsp() as ezsp:
        for color in (
            Rgb(255, 0, 0),
            Rgb(0, 255, 0),
            Rgb(0, 0, 255),
            Rgb(0x12, 0x34, 0x56),
        ):
            await xncp(
                ezsp, XNCP_SET_LED_STATE, bytes([color.red, color.green, color.blue])
            )
            await device.wait_for_led_colors([color] * 4)


@pytest.mark.board("zbt2")
@pytest.mark.parametrize(
    "acceleration", [(0.0, 0.0, 1.0), (0.5, -0.25, 0.75), (-1.5, 2.0, -0.125)]
)
async def test_accelerometer(device, acceleration):
    await device.set_acceleration(*acceleration)

    async with device.ezsp() as ezsp:
        payload = await xncp(ezsp, XNCP_GET_ACCELEROMETER)

    reading = [value / STANDARD_GRAVITY for value in struct.unpack("<3f", payload)]
    # 8 g range at 1024 LSB/g
    assert reading == pytest.approx(acceleration, abs=1 / 1024)
