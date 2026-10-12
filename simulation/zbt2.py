"""A simulated ZBT-2 running a Zigbee NCP firmware in Renode."""

from __future__ import annotations

import asyncio
import contextlib
from collections.abc import AsyncIterator
from dataclasses import dataclass
from pathlib import Path
import struct

import zigpy.config

import bellows.ezsp
import bellows.uart

from simulation.renode import Monitor, free_tcp_port, run_renode

RESC = Path(__file__).parent / "renode" / "zbt2_ncp.resc"

# The emulator runs at roughly 0.15x real time and a reboot takes ~1 s of virtual time, so bellows' 2.5 s wait for
# RSTACK is far too short
RESET_TIMEOUT = 30

XNCP_SET_LED_STATE = 0x0F00
XNCP_GET_ACCELEROMETER = 0x0F01
XNCP_RESPONSE_BIT = 0x8000
XNCP_UNKNOWN = 0xFFFF

STANDARD_GRAVITY = 9.80665


class XncpError(RuntimeError):
    """The NCP rejected an XNCP command."""


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


class Zbt2:
    def __init__(self, monitor: Monitor, uart_port: int) -> None:
        self.monitor = monitor
        self.uart_port = uart_port

    @contextlib.asynccontextmanager
    async def ezsp(self) -> AsyncIterator[bellows.ezsp.EZSP]:
        """Connect bellows to the NCP UART. Connecting resets the NCP."""
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

    async def led_colors(self) -> list[Rgb]:
        """Colors last latched by the WS2812 chain, in chain order."""
        output = await self.monitor.execute("sysbus.eusart1.ws2812 Colors")
        return [Rgb.from_hex(color) for color in output.split()]

    async def wait_for_led_colors(self, expected: list[Rgb], timeout: float = 30) -> None:
        async with asyncio.timeout(timeout):
            while await self.led_colors() != expected:
                await asyncio.sleep(0.5)

    async def set_acceleration(self, x: float, y: float, z: float) -> None:
        """Set the accelerometer reading, in g."""
        for axis, value in zip("XYZ", (x, y, z)):
            await self.monitor.execute(f"sysbus.i2c0.qma Acceleration{axis} {value}")


@contextlib.asynccontextmanager
async def simulate_zbt2(firmware: Path, workdir: Path) -> AsyncIterator[Zbt2]:
    """Boot `firmware` (an NCP `.out`) on a simulated ZBT-2. Renode's log goes to `workdir`."""
    # Renode's `@path` syntax does not cope with every path, e.g. ones with parentheses
    elf = workdir / "firmware.out"
    elf.symlink_to(firmware.resolve())

    uart_port = free_tcp_port()

    async with run_renode(workdir / "renode.log") as monitor:
        await monitor.execute(f"$bin=@{elf}")
        await monitor.execute(f"$uart_port={uart_port}")
        await monitor.execute(f"include @{RESC}")
        await monitor.execute("start")
        yield Zbt2(monitor, uart_port)
