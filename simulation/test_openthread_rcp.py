import asyncio
import struct

import pytest
from aiospinel import PropertyID, ResetReason

from simulation.device import (
    SPINEL_PROP_MAC_SCAN_STATE,
    SPINEL_SCAN_STATE_ENERGY,
    get_property,
    set_property,
)

pytestmark = pytest.mark.fw_type("openthread_rcp")


@pytest.mark.smoke
async def test_reset_and_version(device):
    async with device.spinel() as spinel:
        await spinel.reset(ResetReason.STACK)

        version = await get_property(spinel, PropertyID.NCP_VERSION)
        assert version.startswith(b"SL-OPENTHREAD/")

        major, _minor = await get_property(spinel, PropertyID.PROTOCOL_VERSION)
        assert major == 4


@pytest.mark.parametrize("channel", [11, 15, 20, 26])
async def test_channel(device, channel):
    async with device.spinel() as spinel:
        await spinel.reset(ResetReason.STACK)
        await set_property(spinel, PropertyID.PHY_ENABLED, b"\x01")

        assert await set_property(
            spinel, PropertyID.PHY_CHAN, bytes([channel])
        ) == bytes([channel])
        assert await get_property(spinel, PropertyID.PHY_CHAN) == bytes([channel])


async def test_energy_scan(device):
    async with device.spinel() as spinel:
        await spinel.reset(ResetReason.STACK)
        await set_property(spinel, PropertyID.PHY_ENABLED, b"\x01")

        results = spinel.iter_property_changes(PropertyID.MAC_ENERGY_SCAN_RESULT)
        await set_property(spinel, PropertyID.MAC_SCAN_MASK, bytes([15]))
        await set_property(spinel, PropertyID.MAC_SCAN_PERIOD, struct.pack("<H", 50))
        await set_property(
            spinel, SPINEL_PROP_MAC_SCAN_STATE, bytes([SPINEL_SCAN_STATE_ENERGY])
        )

        async with asyncio.timeout(30):
            result = await anext(results)

        channel, rssi = struct.unpack_from("<Bb", result)
        assert channel == 15
        # Nothing else is transmitting in the simulation
        assert rssi < -100
