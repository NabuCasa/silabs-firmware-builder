"""A simulated board running a firmware in Renode."""

from __future__ import annotations

import asyncio
import contextlib
import struct
from collections.abc import AsyncIterator
from dataclasses import dataclass
from pathlib import Path

import aiospinel.protocol
import bellows.ezsp
import bellows.uart
import zigpy.config
from aiospinel import CommandID, PackedUInt21, PropertyID, SpinelProtocol

from simulation.renode import Monitor, free_tcp_port, run_renode

RENODE_DIR = Path(__file__).parent / "renode"

# The emulator runs at roughly 0.2x real time and a reboot takes ~1 s of virtual time, so the host libraries' reset
# timeouts (2.5 s for bellows, 2 s for aiospinel) are far too short
RESET_TIMEOUT = 30
SPINEL_COMMAND_TIMEOUT = 10

XNCP_SET_LED_STATE = 0x0F00
XNCP_GET_ACCELEROMETER = 0x0F01
XNCP_RESPONSE_BIT = 0x8000

STANDARD_GRAVITY = 9.80665

# aiospinel 1.2.0 defines MAC_SCAN_STATE as 38, which makes it an alias of PHY_RSSI. Spinel puts it at MAC__BEGIN + 0.
SPINEL_PROP_MAC_SCAN_STATE = 48
SPINEL_SCAN_STATE_ENERGY = 2


class XncpError(RuntimeError):
    """The NCP rejected an XNCP command."""


class SpinelError(RuntimeError):
    """The RCP answered a property command with something other than the property."""


@dataclass(frozen=True)
class Rgb:
    red: int
    green: int
    blue: int

    @classmethod
    def from_hex(cls, value: str) -> Rgb:
        return cls(int(value[1:3], 16), int(value[3:5], 16), int(value[5:7], 16))


async def xncp(ezsp: bellows.ezsp.EZSP, command_id: int, payload: bytes = b"") -> bytes:
    """Send a raw XNCP command and return the response payload."""
    status, data = await ezsp.customFrame(struct.pack("<HB", command_id, 0) + payload)
    response_id, xncp_status = struct.unpack_from("<HB", data)

    if response_id != command_id | XNCP_RESPONSE_BIT or xncp_status != 0:
        raise XncpError(
            f"XNCP 0x{command_id:04X}: response 0x{response_id:04X}, status {xncp_status}, EZSP {status}"
        )
    return data[3:]


async def _property_command(
    spinel: SpinelProtocol,
    command_id: CommandID,
    property_id: PropertyID | int,
    value: bytes,
) -> bytes:
    rsp = await spinel.send_command(
        command_id,
        PackedUInt21(property_id).serialize() + value,
        timeout=SPINEL_COMMAND_TIMEOUT,
    )
    rsp_property_id, rsp_value = PackedUInt21.deserialize(rsp.data)

    if rsp_property_id != property_id:
        raise SpinelError(
            f"{command_id.name} {property_id!r}: got property {rsp_property_id} {rsp_value.hex()}"
        )
    return rsp_value


async def get_property(spinel: SpinelProtocol, property_id: PropertyID | int) -> bytes:
    return await _property_command(spinel, CommandID.PROP_VALUE_GET, property_id, b"")


async def set_property(
    spinel: SpinelProtocol, property_id: PropertyID | int, value: bytes
) -> bytes:
    return await _property_command(spinel, CommandID.PROP_VALUE_SET, property_id, value)


class Device:
    def __init__(self, monitor: Monitor, uart_port: int) -> None:
        self.monitor = monitor
        self.uart_port = uart_port

    @contextlib.asynccontextmanager
    async def ezsp(self) -> AsyncIterator[bellows.ezsp.EZSP]:
        """Connect bellows to a Zigbee NCP. Connecting resets the NCP."""
        ezsp = bellows.ezsp.EZSP(
            zigpy.config.SCHEMA_DEVICE(
                {zigpy.config.CONF_DEVICE_PATH: f"socket://127.0.0.1:{self.uart_port}"}
            )
        )

        reset_timeout = bellows.uart.RESET_TIMEOUT
        bellows.uart.RESET_TIMEOUT = RESET_TIMEOUT
        try:
            await ezsp.connect(use_thread=False)
            try:
                yield ezsp
            finally:
                await ezsp.disconnect()
        finally:
            bellows.uart.RESET_TIMEOUT = reset_timeout

    @contextlib.asynccontextmanager
    async def spinel(self) -> AsyncIterator[SpinelProtocol]:
        """Connect aiospinel to an OpenThread RCP. Connecting does not reset the RCP."""
        reset_timeout = aiospinel.protocol.RESET_TIMEOUT
        aiospinel.protocol.RESET_TIMEOUT = RESET_TIMEOUT
        try:
            _, protocol = await asyncio.get_running_loop().create_connection(
                SpinelProtocol, "127.0.0.1", self.uart_port
            )
            await protocol.wait_until_connected()
            try:
                yield protocol
            finally:
                await protocol.disconnect()
        finally:
            aiospinel.protocol.RESET_TIMEOUT = reset_timeout

    async def led_colors(self) -> list[Rgb]:
        """Colors last latched by the ZBT-2 WS2812 chain, in chain order."""
        output = await self.monitor.execute("sysbus.eusart1.ws2812 Colors")
        return [Rgb.from_hex(color) for color in output.split()]

    async def wait_for_led_colors(
        self, expected: list[Rgb], timeout: float = 30
    ) -> None:
        async with asyncio.timeout(timeout):
            while await self.led_colors() != expected:
                await asyncio.sleep(0.5)

    async def set_acceleration(self, x: float, y: float, z: float) -> None:
        """Set the ZBT-2 accelerometer reading, in g."""
        for axis, value in zip("XYZ", (x, y, z)):
            await self.monitor.execute(f"sysbus.i2c0.qma Acceleration{axis} {value}")


@contextlib.asynccontextmanager
async def simulate(firmware: Path, board: str, workdir: Path) -> AsyncIterator[Device]:
    """Boot a firmware `.out` on a simulated board. Renode's log goes to `workdir`."""
    # Renode's `@path` syntax does not cope with every path, e.g. ones with parentheses
    elf = workdir / "firmware.out"
    elf.symlink_to(firmware.resolve())

    uart_port = free_tcp_port()

    async with run_renode(workdir / "renode.log") as monitor:
        await monitor.execute(f"$bin=@{elf}")
        await monitor.execute(f"$uart_port={uart_port}")
        await monitor.execute(f"include @{RENODE_DIR / board}.resc")
        await monitor.execute("start")
        yield Device(monitor, uart_port)
