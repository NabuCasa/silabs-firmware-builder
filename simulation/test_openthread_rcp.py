import asyncio
import struct

from aiospinel import PropertyID, ResetReason
import pytest

from simulation.zbt2 import (
    SPINEL_PROP_MAC_SCAN_STATE,
    SPINEL_SCAN_STATE_ENERGY,
    get_property,
    set_property,
    simulate_zbt2,
)


async def test_reset_and_version(firmware, tmp_path):
    async with simulate_zbt2(firmware, tmp_path) as zbt2:
        async with zbt2.spinel() as spinel:
            await spinel.reset(ResetReason.STACK)

            version = await get_property(spinel, PropertyID.NCP_VERSION)
            assert version.startswith(b"SL-OPENTHREAD/")

            major, minor = await get_property(spinel, PropertyID.PROTOCOL_VERSION)
            assert major == 4


@pytest.mark.parametrize("channel", [11, 15, 20, 26])
async def test_channel(firmware, tmp_path, channel):
    async with simulate_zbt2(firmware, tmp_path) as zbt2:
        async with zbt2.spinel() as spinel:
            await spinel.reset(ResetReason.STACK)
            await set_property(spinel, PropertyID.PHY_ENABLED, b"\x01")

            assert await set_property(spinel, PropertyID.PHY_CHAN, bytes([channel])) == bytes([channel])
            assert await get_property(spinel, PropertyID.PHY_CHAN) == bytes([channel])


async def test_energy_scan(firmware, tmp_path):
    async with simulate_zbt2(firmware, tmp_path) as zbt2:
        async with zbt2.spinel() as spinel:
            await spinel.reset(ResetReason.STACK)
            await set_property(spinel, PropertyID.PHY_ENABLED, b"\x01")

            results = spinel.iter_property_changes(PropertyID.MAC_ENERGY_SCAN_RESULT)
            await set_property(spinel, PropertyID.MAC_SCAN_MASK, bytes([15]))
            await set_property(spinel, PropertyID.MAC_SCAN_PERIOD, struct.pack("<H", 50))
            await set_property(spinel, SPINEL_PROP_MAC_SCAN_STATE, bytes([SPINEL_SCAN_STATE_ENERGY]))

            async with asyncio.timeout(30):
                result = await anext(results)

            channel, rssi = struct.unpack_from("<Bb", result)
            assert channel == 15
            # Nothing else is transmitting in the simulation
            assert rssi < -100
