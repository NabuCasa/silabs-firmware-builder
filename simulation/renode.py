"""Run Renode headless and drive it through its telnet monitor."""

from __future__ import annotations

import asyncio
import contextlib
import os
import re
import shutil
import signal
import socket
from collections.abc import AsyncIterator
from pathlib import Path

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
TELNET_NEGOTIATION = re.compile(rb"\xff[\xfb-\xfe].|\xff\xfa.*?\xff\xf0", re.DOTALL)
PROMPT = re.compile(r"\(([\w-]+)\) $")
ERRORS = ("There was an error executing command", "No such command")

STARTUP_TIMEOUT = 60
COMMAND_TIMEOUT = 60


class MonitorError(RuntimeError):
    """A monitor command failed."""


def free_tcp_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Monitor:
    def __init__(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        self._reader = reader
        self._writer = writer
        self._lock = asyncio.Lock()

    async def _read_until_prompt(self) -> str:
        raw = b""
        async with asyncio.timeout(COMMAND_TIMEOUT):
            while True:
                raw += await self._reader.read(65536)
                text = ANSI_ESCAPE.sub(
                    "", TELNET_NEGOTIATION.sub(b"", raw).decode("latin-1")
                )
                text = text.replace("\r", "")
                if PROMPT.search(text):
                    return PROMPT.sub("", text)

    async def execute(self, command: str) -> str:
        """Run one monitor command and return its output."""
        async with self._lock:
            self._writer.write(command.encode() + b"\n")
            await self._writer.drain()
            output = await self._read_until_prompt()

        # The monitor echoes the command on the first line
        output = output.split("\n", 1)[1] if "\n" in output else ""
        if any(error in output for error in ERRORS):
            raise MonitorError(f"{command!r}: {output.strip()}")
        return output.strip()


@contextlib.asynccontextmanager
async def run_renode(log_path: Path) -> AsyncIterator[Monitor]:
    """Start a headless Renode and yield a connected monitor."""
    port = free_tcp_port()

    with log_path.open("wb") as log:
        process = await asyncio.create_subprocess_exec(
            shutil.which("renode"),
            "--disable-gui",
            "--port",
            str(port),
            stdout=log,
            stderr=asyncio.subprocess.STDOUT,
            # `renode` is a shell wrapper around dotnet, so the whole group has to be killed
            start_new_session=True,
        )

        try:
            async with asyncio.timeout(STARTUP_TIMEOUT):
                while True:
                    try:
                        reader, writer = await asyncio.open_connection(
                            "127.0.0.1", port
                        )
                        break
                    except ConnectionRefusedError:
                        await asyncio.sleep(0.2)

            monitor = Monitor(reader, writer)
            await monitor._read_until_prompt()

            try:
                yield monitor
            finally:
                writer.close()
        finally:
            os.killpg(process.pid, signal.SIGKILL)
            await process.wait()
